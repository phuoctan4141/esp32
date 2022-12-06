#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include <string.h>
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "jsmn.h"

#include "http.h"
#include "wifi.h"

#define RECV_BUFFER_SIZE 64

#define WEB_SERVER CONFIG_WEB_SERVER
#define WEB_PORT CONFIG_WEB_PORT
#define WEB_URL CONFIG_WEB_URL

#define LAT CONFIG_LAT
#define LON CONFIG_LON

#define OPENWEATHERMAP_API_KEY CONFIG_OPENWEATHERMAP_API_KEY

static const char *REQUEST = "GET " WEB_URL "?lat=" LAT "&lon=" LON "&appid=" OPENWEATHERMAP_API_KEY " HTTP/1.1\n"
                             "Host: " WEB_SERVER "\n"
                             "Connection: close\n"
                             "User-Agent: esp-idf/1.0 esp32\n"
                             "\n";

static const char *TAG = "http";
static const char *TAG_WEATHER = "weather";

weather_data weather;
http_client_data client = {0};

/* Collect chunks of data received from server
   into complete message and save it in proc_buf
 */
static void process_chunk(void)
{
    int proc_buf_new_size = client.proc_buf_size + client.recv_buf_size;
    char *copy_from;

    if (client.proc_buf == NULL)
    {
        client.proc_buf = malloc(proc_buf_new_size);
        copy_from = client.proc_buf;
    }
    else
    {
        proc_buf_new_size -= 1; // chunks of data are '\0' terminated
        client.proc_buf = realloc(client.proc_buf, proc_buf_new_size);
        copy_from = client.proc_buf + proc_buf_new_size - client.recv_buf_size;
    }
    if (client.proc_buf == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory");
    }
    client.proc_buf_size = proc_buf_new_size;
    memcpy(copy_from, client.recv_buf, client.recv_buf_size);
}

/* Out of HTTP response return pointer to response body
   Function return NULL if end of header cannot be identified
 */
const char *find_response_body(char *response)
{
    // Identify end of the response headers
    // http://stackoverflow.com/questions/11254037/how-to-know-when-the-http-headers-part-is-ended
    char *eol;                 // end of line
    char *bol;                 // beginning of line
    bool nheaderfound = false; // end of response headers has been found

    bol = response;
    while ((eol = index(bol, '\n')) != NULL)
    {
        // update bol based upon the value of eol
        bol = eol + 1;
        // test if end of headers has been reached
        if ((!(strncmp(bol, "\r\n", 2))) || (!(strncmp(bol, "\n", 1))))
        {
            // note that end of headers has been found
            nheaderfound = true;
            // update the value of bol to reflect the beginning of the line
            // immediately after the headers
            if (bol[0] != '\n')
            {
                bol += 1;
            }
            bol += 1;
            break;
        }
    }
    if (nheaderfound)
    {
        return bol;
    }
    else
    {
        return NULL;
    }
}

static int jsoneq(const char *json, jsmntok_t *tok, const char *s)
{
    if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
        strncmp(json + tok->start, s, tok->end - tok->start) == 0)
    {
        return 0;
    }
    return -1;
}

static bool process_response_body(const char *body)
{
    /* Using great little JSON parser http://zserge.com/jsmn.html
       find specific weather information:
         - Humidity,
         - Temperature,
         - Pressure
       in HTTP response body that happens to be a JSON string
       Return true if phrasing was successful or false if otherwise
     */

#define JSON_MAX_TOKENS 100
    jsmn_parser parser;
    jsmntok_t t[JSON_MAX_TOKENS];
    jsmn_init(&parser);
    int r = jsmn_parse(&parser, body, strlen(body), t, JSON_MAX_TOKENS);
    if (r < 0)
    {
        ESP_LOGE(TAG, "JSON parse error %d", r);
        return false;
    }
    if (r < 1 || t[0].type != JSMN_OBJECT)
    {
        ESP_LOGE(TAG, "JSMN_OBJECT expected");
        return false;
    }
    else
    {
        ESP_LOGD(TAG, "Token(s) found %d", r);
        char subbuff[8];
        int str_length;
        for (int i = 1; i < r; i++)
        {
            if (jsoneq(body, &t[i], "humidity") == 0)
            {
                str_length = t[i + 1].end - t[i + 1].start;
                memcpy(subbuff, body + t[i + 1].start, str_length);
                subbuff[str_length] = '\0';
                weather.humidity = atoi(subbuff);
                i++;
            }
            else if (jsoneq(body, &t[i], "temp") == 0)
            {
                str_length = t[i + 1].end - t[i + 1].start;
                memcpy(subbuff, body + t[i + 1].start, str_length);
                subbuff[str_length] = '\0';
                weather.temperature = atof(subbuff);
                i++;
            }
            else if (jsoneq(body, &t[i], "pressure") == 0)
            {
                str_length = t[i + 1].end - t[i + 1].start;
                memcpy(subbuff, body + t[i + 1].start, str_length);
                subbuff[str_length] = '\0';
                weather.pressure = atof(subbuff);
                i++;
            }
        }
        return true;
    }
}

/* Get and Set data from HTTP response */
void weather_handler(void)
{
    bool weather_data_phrased = false;

    const char *response_body = find_response_body(client.proc_buf);

    if (response_body != NULL)
    {
        weather_data_phrased = process_response_body(response_body);
    }
    else
    {
        ESP_LOGE(TAG, "No HTTP header found");
        weather.isExist = false;
    }

    if (weather_data_phrased)
    {
        ESP_LOGD(TAG_WEATHER,
                 "humidity: %d, temperature: %0.2f, pressure: %0.2f \n",
                 weather.humidity, weather.temperature, weather.pressure);

        weather.isExist = true;
    }

    free(client.proc_buf);
    client.proc_buf = NULL;
    client.proc_buf_size = 0;

    ESP_LOGD(TAG, "Free heap %u", xPortGetFreeHeapSize());
}

// HTTP request and receive data
static void http_get_task(void *pvParameters)
{
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;
    int s, r;
    char recv_buf[RECV_BUFFER_SIZE];

    client.recv_buf = recv_buf;
    client.recv_buf_size = sizeof(recv_buf);

    while (1)
    {
        bool isLive = network_is_alive();

        if (isLive)
        {
            int err = getaddrinfo(WEB_SERVER, WEB_PORT, &hints, &res);

            if (err != 0 || res == NULL)
            {
                ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                continue;
            }

            /* Code to print the resolved IP.
               Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r for "real" code */
            addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
            ESP_LOGD(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

            s = socket(res->ai_family, res->ai_socktype, 0);
            if (s < 0)
            {
                ESP_LOGE(TAG, "... Failed to allocate socket.");
                freeaddrinfo(res);
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                continue;
            }
            ESP_LOGD(TAG, "... allocated socket");

            if (connect(s, res->ai_addr, res->ai_addrlen) != 0)
            {
                ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
                close(s);
                freeaddrinfo(res);
                vTaskDelay(4000 / portTICK_PERIOD_MS);
                continue;
            }

            ESP_LOGD(TAG, "... connected");
            freeaddrinfo(res);

            if (write(s, REQUEST, strlen(REQUEST)) < 0)
            {
                ESP_LOGE(TAG, "... socket send failed");
                close(s);
                vTaskDelay(4000 / portTICK_PERIOD_MS);
                continue;
            }
            ESP_LOGD(TAG, "... socket send success");

            struct timeval receiving_timeout;
            receiving_timeout.tv_sec = 5;
            receiving_timeout.tv_usec = 0;
            if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
                           sizeof(receiving_timeout)) < 0)
            {
                ESP_LOGE(TAG, "... failed to set socket receiving timeout");
                close(s);
                vTaskDelay(4000 / portTICK_PERIOD_MS);
                continue;
            }
            ESP_LOGD(TAG, "... set socket receiving timeout success");

            /* Read HTTP response */
            do
            {
                bzero(recv_buf, sizeof(recv_buf));
                r = read(s, recv_buf, sizeof(recv_buf) - 1);
                process_chunk();

            } while (r > 0);

            weather_handler();

            ESP_LOGD(TAG, "... done reading from socket. Last read return=%d errno=%d.", r, errno);
            close(s);
        }
        else
        {
            ESP_LOGE(TAG, "Not connect !!!");
        }

        for (int countdown = 10; countdown >= 0; countdown--)
        {
            ESP_LOGD(TAG, "%d... ", countdown);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        ESP_LOGD(TAG, "Starting again!");
    }
}

void setupHTTP(void)
{
    weather.isExist = false;
    xTaskCreate(&http_get_task, "http_get_task", 4096, NULL, 5, NULL);
}