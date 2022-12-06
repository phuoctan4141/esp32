#ifndef WIFI_H
#define WIFI_H

#ifdef __cplusplus
extern "C"
{
#endif

    void wifi_init_sta(void);
    bool network_is_alive(void);
    void setupWiFi(void);

#ifdef __cplusplus
}
#endif

#endif