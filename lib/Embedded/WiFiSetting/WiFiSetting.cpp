#include "WiFiSetting.h"

WiFiSetting::WiFiSetting()
{
    esp_log_level_set(_TAG, ESP_LOG_VERBOSE);
}

/**
 * begin the connection
 * 
 * @param[in]   wifiParams WifiParams data structure
*/
void WiFiSetting::begin(WifiParams &wifiParams)
{
    ESP_LOGI(_TAG, "WiFi setting begin");
    _modeType = static_cast<mode_type>(wifiParams.mode);
    _apGateway = String(wifiParams.softApParams.gateway.data());
    switch (_modeType)
    {
    case AP:
        ESP_LOGI(_TAG, "WIFI Mode AP\n");
        WiFi.mode(WIFI_MODE_AP);
        break;
    case STATION:
        ESP_LOGI(_TAG, "WIFI Mode STATION\n");
        WiFi.mode(WIFI_MODE_STA);
        break;
    case AP_STATION:
        ESP_LOGI(_TAG, "WIFI Mode AP + STATION\n");
        WiFi.mode(WIFI_MODE_APSTA);
        break;
    default:
        break;
    }

    if (_modeType == STATION)
    {
        IPAddress ip;
        IPAddress gateway;
        IPAddress subnet;
        switch (wifiParams.params.server)
        {
        case STATIC:
            ESP_LOGI(_TAG, "Station Static IP\n");
            ip.fromString(wifiParams.params.ip.data());
            gateway.fromString(wifiParams.params.gateway.data());
            subnet.fromString(wifiParams.params.subnet.data());
            WiFi.config(ip, gateway, subnet);
            break;
        case DHCP :
            ESP_LOGI(_TAG, "Station Dynamic IP\n");
        default:
            break;
        }
        if (String(wifiParams.params.ssid.data()) == "")
        {
            ESP_LOGE(_TAG, "Station SSID not provided");
            return;
        }
        else 
        {
            if (String(wifiParams.params.pass.data()) == "")
            {
                WiFi.begin(String(wifiParams.params.ssid.data()));
            }
            else
            {
                WiFi.begin(String(wifiParams.params.ssid.data()), String(wifiParams.params.pass.data()));
            }
        }        
    }
    else if (_modeType == AP)
    {
        IPAddress ip;
        IPAddress gateway;
        IPAddress subnet;
        switch (wifiParams.params.server)
        {
        case STATIC:
            ESP_LOGI(_TAG, "AP Static IP\n");
            ip.fromString(wifiParams.params.ip.data());
            gateway.fromString(wifiParams.params.gateway.data());
            subnet.fromString(wifiParams.params.subnet.data());
            WiFi.softAPConfig(ip, gateway, subnet);
            break;
        case DHCP:
            ESP_LOGI(_TAG, "AP Dynamic IP\n");
        default:
            break;
        }
        if (String(wifiParams.params.ssid.data()) == "")
        {
            ESP_LOGE(_TAG, "AP SSID not provided");
            return;
        }
        else 
        {
            if (String(wifiParams.params.pass.data()) == "")
            {
                WiFi.softAP(String(wifiParams.params.ssid.data()));
            }
            else
            {
                WiFi.softAP(String(wifiParams.params.ssid.data()), String(wifiParams.params.pass.data()));
            }
        }
        
    }
    else if (_modeType == AP_STATION)
    {
        IPAddress ip;
        IPAddress gateway;
        IPAddress subnet;
        switch (wifiParams.params.server)
        {
        case STATIC:
            ESP_LOGI(_TAG, "Station Static IP\n");
            ip.fromString(wifiParams.params.ip.data());
            gateway.fromString(wifiParams.params.gateway.data());
            subnet.fromString(wifiParams.params.subnet.data());
            WiFi.config(ip, gateway, subnet);
            break;
        case DHCP:
            ESP_LOGI(_TAG, "Station Dynamic IP\n");
        default:
            break;
        }

        ESP_LOGI(_TAG, "AP Config IP\n");
        ip.fromString(wifiParams.softApParams.ip.data());
        gateway.fromString(wifiParams.softApParams.gateway.data());
        subnet.fromString(wifiParams.softApParams.subnet.data());
        WiFi.softAPConfig(ip, gateway, subnet);

        if (String(wifiParams.softApParams.ssid.data()) == "")
        {
            ESP_LOGE(_TAG, "AP SSID not provided");
        }
        else 
        {
            if (String(wifiParams.softApParams.pass.data()) == "")
            {
                WiFi.softAP(String(wifiParams.softApParams.ssid.data()));
            }
            else
            {
                WiFi.softAP(String(wifiParams.softApParams.ssid.data()), String(wifiParams.softApParams.pass.data()));
            }
        }
        
        if (String(wifiParams.params.ssid.data()) == "")
        {
            ESP_LOGE(_TAG, "Station SSID not provided");
        }
        else
        {
            if (String(wifiParams.params.pass.data()) == "")
            {
                WiFi.begin(String(wifiParams.params.ssid.data()));
            }
            else
            {
                WiFi.begin(String(wifiParams.params.ssid.data()), String(wifiParams.params.pass.data()));
            }
        }
    }
}

/**
 * get mode
 * 
 * @return   mode_type enum
*/
mode_type WiFiSetting::getMode()
{
    return _modeType;
}

/**
 * get server
 * 
 * @return   server_type enum
*/
server_type WiFiSetting::getStationServer()
{
    return _stationServerType;
}

/**
 * get ip
 * 
 * @return   ip
*/
String WiFiSetting::getIp()
{
    return WiFi.localIP().toString();
}

/**
 * get gateway ip
 * 
 * @return   gateway ip
*/
String WiFiSetting::getGateway()
{
    return WiFi.gatewayIP().toString();
}

/**
 * get subnet mask
 * 
 * @return   subnet mask
*/
String WiFiSetting::getSubnet()
{
    return WiFi.subnetMask().toString();
}

/**
 * get ssid
 * 
 * @return   ssid
*/
String WiFiSetting::getSsid()
{
    return WiFi.SSID();
}

/**
 * get ap server
 * 
 * @return   server_type enum
*/
server_type WiFiSetting::getApServer()
{
    return _apServerType;
}

/**
 * get ap ip
 * 
 * @return   ap ip
*/
String WiFiSetting::getApIp()
{
    return WiFi.softAPIP().toString();
}

/**
 * get ap subnet mask
 * 
 * @return   ap subnet mask
*/
String WiFiSetting::getApSubnet()
{
    return WiFi.softAPSubnetMask().toString();
}

/**
 * get ap ssid
 * 
 * @return   ap ssid
*/
String WiFiSetting::getApSsid()
{
    return WiFi.softAPSSID();
}

WiFiSetting::~WiFiSetting()
{
}