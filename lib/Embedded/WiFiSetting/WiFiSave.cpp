#include "WiFiSave.h"

WiFiSave::WiFiSave()
{
}

void WiFiSave::begin(String name)
{
    Preferences preferences;
    preferences.begin(name.c_str());
    _name = name;
    if (!preferences.isKey("init_flg")) // get init flag to detect the first time init
    {
        ESP_LOGI(_TAG, "Create default");
        createDefault();
        copy();
    }
    if (preferences.getBool("rst_flg")) // get reset flag to revert the user parameter into default
    {
        ESP_LOGI(_TAG, "Revert to default");
        copy();
        preferences.putBool("rst_flg", false);
    }
    _isActive = true;
    printDefault();
    printUser();
    writeShadow();
}

void WiFiSave::createDefault()
{
    String ssid = "esp32-" + WiFi.macAddress();
    ssid.replace(":", "");
    Preferences preferences;
    preferences.begin(_name.c_str());
    preferences.putUChar("d_mode", 1); // set to AP
    preferences.putUChar("d_svr", 1); // set to STATIC
    preferences.putString("d_ip", "192.168.4.1");
    preferences.putString("d_gateway", "192.168.4.1");
    preferences.putString("d_subnet", "255.255.255.0");
    preferences.putString("d_ssid", ssid);
    preferences.putString("d_pwd", "esp32-default");
    preferences.putBool("init_flg", true);
    preferences.putBool("rst_flg", false);
    preferences.end();
}

void WiFiSave::copy()
{
    Preferences preferences;
    preferences.begin(_name.c_str());
    preferences.putUChar("u_mode", preferences.getUChar("d_mode"));
    preferences.putUChar("u_svr", preferences.getUChar("d_svr"));
    preferences.putString("u_ip", preferences.getString("d_ip"));
    preferences.putString("u_gateway", preferences.getString("d_gateway"));
    preferences.putString("u_subnet", preferences.getString("d_subnet"));
    preferences.putString("u_ssid", preferences.getString("d_ssid"));
    preferences.putString("u_pwd", preferences.getString("d_pwd"));
    preferences.end();
}

void WiFiSave::setMode(uint8_t mode)
{
    if (_isActive)
    {
        _shadowParameter.mode = mode;
    }

}

void WiFiSave::setServer(uint8_t serverType)
{
    if (_isActive)
    {
        _shadowParameter.server = serverType;
    }
}

void WiFiSave::setIp(String ip)
{
    if (_isActive)
    {
        _shadowParameter.ip = ip;
    }
}

void WiFiSave::setGateway(String ip)
{
    if (_isActive)
    {
        _shadowParameter.gateway = ip;
    }
}

void WiFiSave::setSubnet(String ip)
{
    if (_isActive)
    {
        _shadowParameter.subnet = ip;
    }
}

void WiFiSave::setSsid(String ssid)
{
    if (_isActive)
    {
        _shadowParameter.ssid = ssid;
    }
}

void WiFiSave::setPassword(String password)
{
    if (_isActive)
    {
        _shadowParameter.password = password;
    }
}

void WiFiSave::save()
{
    if (_isActive)
    {
        Preferences preferences;
        preferences.begin(_name.c_str());
        preferences.putUChar("u_mode", _shadowParameter.mode);
        preferences.putUChar("u_svr", _shadowParameter.server);
        preferences.putString("u_ip", _shadowParameter.ip);
        preferences.putString("u_gateway", _shadowParameter.gateway);
        preferences.putString("u_subnet", _shadowParameter.subnet);
        preferences.putString("u_ssid", _shadowParameter.ssid);
        preferences.putString("u_pwd", _shadowParameter.password);
        preferences.end();
    }
}

void WiFiSave::writeShadow()
{
    if (_isActive)
    {
        Preferences preferences;
        preferences.begin(_name.c_str());
        _shadowParameter.mode = preferences.getUChar("u_mode");
        _shadowParameter.server = preferences.getUChar("u_svr");
        _shadowParameter.ip = preferences.getString("u_ip");
        _shadowParameter.gateway = preferences.getString("u_gateway");
        _shadowParameter.subnet = preferences.getString("u_subnet");
        _shadowParameter.ssid = preferences.getString("u_ssid");
        _shadowParameter.password = preferences.getString("u_pwd");
        preferences.end();
    }
}

void WiFiSave::cancel()
{
    writeShadow();
}

void WiFiSave::reset()
{
    if (_isActive)
    {
        Preferences preferences;
        preferences.begin(_name.c_str());
        preferences.putBool("rst_flg", true);
        preferences.end();
        _isActive = false;
        restart();
    }
}

void WiFiSave::clear()
{
    if (_isActive)
    {
        Preferences preferences;
        preferences.begin(_name.c_str());
        preferences.clear();
        preferences.end();
        _isActive = false;
    }
}

void WiFiSave::restart()
{
    begin(_name);
}

/**
 * Get mode of the configuration
 * 
 * return   mode of the configuration, 1 for AP, 2 for Station, 3 for AP + Station
*/
uint8_t WiFiSave::getMode()
{
    if (_isActive)
    {
        Preferences preferences;
        preferences.begin(_name.c_str());
        uint8_t value = preferences.getUChar("u_mode");
        preferences.end();
        return value;
    }
    return 0;
}

uint8_t WiFiSave::getServer()
{
    if (_isActive)
    {
        Preferences preferences;
        preferences.begin(_name.c_str());
        uint8_t value = preferences.getUChar("u_svr");
        preferences.end();
        return value;
    }
    return 0;
}

String WiFiSave::getIp()
{
    if (_isActive)
    {
        Preferences preferences;
        preferences.begin(_name.c_str());
        String value = preferences.getString("u_ip");
        preferences.end();
        return value;
    }
    return "";
}

String WiFiSave::getGateway()
{
    if (_isActive)
    {
        Preferences preferences;
        preferences.begin(_name.c_str());
        String value = preferences.getString("u_gateway");
        preferences.end();
        return value;
    }
    return "";
}

String WiFiSave::getSubnet()
{
    if (_isActive)
    {
        Preferences preferences;
        preferences.begin(_name.c_str());
        String value = preferences.getString("u_subnet");
        preferences.end();
        return value;
    }
    return "";
}

String WiFiSave::getSsid()
{
    if (_isActive)
    {
        Preferences preferences;
        preferences.begin(_name.c_str());
        String value = preferences.getString("u_ssid");
        preferences.end();
        return value;
    }
    return "";
}

String WiFiSave::getPassword()
{
    if (_isActive)
    {
        Preferences preferences;
        preferences.begin(_name.c_str());
        String value = preferences.getString("u_pwd");
        preferences.end();
        return value;
    }
    return "";
}

void WiFiSave::printDefault()
{
    Preferences preferences;
    preferences.begin(_name.c_str());
    ESP_LOGI(_TAG, "d_mode : %d\n", preferences.getUChar("d_mode"));
    ESP_LOGI(_TAG, "d_svr : %d\n", preferences.getUChar("d_svr"));
    ESP_LOGI(_TAG, "d_ip : %s\n", preferences.getString("d_ip").c_str());
    ESP_LOGI(_TAG, "d_gateway : %s\n", preferences.getString("d_gateway").c_str());
    ESP_LOGI(_TAG, "d_subnet : %s\n", preferences.getString("d_subnet").c_str());
    ESP_LOGI(_TAG, "d_ssid : %s\n", preferences.getString("d_ssid").c_str());
    ESP_LOGI(_TAG, "d_pwd : %s\n", preferences.getString("d_pwd").c_str());
    preferences.end();
}

void WiFiSave::printUser()
{
    Preferences preferences;
    preferences.begin(_name.c_str());
    ESP_LOGI(_TAG, "u_mode : %d\n", preferences.getUChar("u_mode"));
    ESP_LOGI(_TAG, "u_svr : %d\n", preferences.getUChar("u_svr"));
    ESP_LOGI(_TAG, "u_ip : %s\n", preferences.getString("u_ip").c_str());
    ESP_LOGI(_TAG, "u_gateway : %s\n", preferences.getString("u_gateway").c_str());
    ESP_LOGI(_TAG, "u_subnet : %s\n", preferences.getString("u_subnet").c_str());
    ESP_LOGI(_TAG, "u_ssid : %s\n", preferences.getString("u_ssid").c_str());
    ESP_LOGI(_TAG, "u_pwd : %s\n", preferences.getString("u_pwd").c_str());
    preferences.end();
}

WiFiSave::~WiFiSave()
{
}