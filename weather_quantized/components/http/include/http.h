#ifndef HTTP_H
#define HTTP_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct
    {
        char *recv_buf;    /*!< Pointer to a receive buffer, data from the socket are collected here */
        int recv_buf_size; /*!< Size of the receive buffer */
        char *proc_buf;    /*!< Pointer to processing buffer,  chunks of data from receive buffer and collected here. */
        int proc_buf_size; /*!< Size of processing buffer*/
    } http_client_data;

    typedef struct
    {
        unsigned int humidity;
        float temperature;
        float pressure;
        bool  isExist;
    } weather_data;

    extern weather_data weather;

    void setupHTTP(void);

#ifdef __cplusplus
}
#endif

#endif