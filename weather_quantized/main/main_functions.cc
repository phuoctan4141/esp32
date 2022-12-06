#include "main_functions.h"

#include "wifi.h"
#include "http.h"
#include "heat.h"

void setup()
{
    setupWiFi();
 
    setupHTTP();

    setupHEAT();
}

void loop() {}