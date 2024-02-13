#ifndef WIFI_SAVE_H
#define WIFI_SAVE_H

#include <Arduino.h>
#include <Preferences.h>
#include "WiFidef.h"
#include "LittleFS.h"
#include <WiFi.h>

struct WifiParameterData {
    uint8_t mode;
    uint8_t server;
    String ip;
    String gateway;
    String subnet;
    String ssid;
    String password;
};

class WiFiSave
{
private:
    /* data */
    const char* _TAG = "WiFi Save";
    WifiParameterData _shadowParameter;
    String _name;
    bool _isActive = false;
    void copy();
    void createDefault();
    void writeShadow();

public:
    WiFiSave();
    void printDefault();
    void printUser();
    
    void begin(String name);
    void save();
    void cancel();
    void reset();
    void restart();
    void clear();
    void setMode(uint8_t mode);

    void setServer(uint8_t serverType);
    void setIp(String ip);
    void setGateway(String ip);
    void setSubnet(String ip);
    void setSsid(String ssid);
    void setPassword(String password);

    uint8_t getMode();

    uint8_t getServer();
    String getIp();
    String getGateway();
    String getSubnet();
    String getSsid();
    String getPassword();

    ~WiFiSave();
};


#endif