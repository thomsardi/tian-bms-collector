#include "WiFiSave.h"

WiFiSave::WiFiSave()
{
}

/**
 * begin the preference namespace, write the default setting and init flag
 * 
 * @param[in]   name    name for preference namespace
*/
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

/**
 * create default parameter
*/
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

/**
 * copy default parameter into user parameter
*/
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

/**
 * set wifi mode
 * 
 * @param[in]   mode  1 = AP, 2 = STATION, 3 = AP+STATION
*/
void WiFiSave::setMode(uint8_t mode)
{
    if (_isActive)
    {
        _shadowParameter.mode = mode;
    }

}

/**
 * set wifi server
 * 
 * @param[in]   serverType  1 = STATIC, 2 = DHCP
*/
void WiFiSave::setServer(uint8_t serverType)
{
    if (_isActive)
    {
        _shadowParameter.server = serverType;
    }
}

/**
 * set wifi static IP
 * 
 * @param[in]   ip 
*/
void WiFiSave::setIp(String ip)
{
    if (_isActive)
    {
        _shadowParameter.ip = ip;
    }
}

/**
 * set wifi gateway IP
 * 
 * @param[in]   ip 
*/
void WiFiSave::setGateway(String ip)
{
    if (_isActive)
    {
        _shadowParameter.gateway = ip;
    }
}

/**
 * set wifi subnet mask
 * 
 * @param[in]   ip 
*/
void WiFiSave::setSubnet(String ip)
{
    if (_isActive)
    {
        _shadowParameter.subnet = ip;
    }
}

/**
 * set wifi ssid
 * 
 * @param[in]   ip 
*/
void WiFiSave::setSsid(String ssid)
{
    if (_isActive)
    {
        _shadowParameter.ssid = ssid;
    }
}

/**
 * set wifi password
 * 
 * @param[in]   ip 
*/
void WiFiSave::setPassword(String password)
{
    if (_isActive)
    {
        _shadowParameter.password = password;
    }
}

/**
 * write and save into preference memory
*/
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

/**
 * write shadow register from user parameter
*/
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

/**
 * cancel the recent input and revert back into last known parameter
*/
void WiFiSave::cancel()
{
    writeShadow();
}

/**
 * reset into default parameter
*/
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

/**
 * restart the object
*/
void WiFiSave::restart()
{
    begin(_name);
}

/**
 * clear all preferences keys
*/
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



/**
 * Get mode of the configuration
 * 
 * @return   mode of the configuration, 1 for AP, 2 for Station, 3 for AP + Station
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

/**
 * Get server of the configuration
 * 
 * @return   server of the configuration, 1 for STATIC, 2 for DHCP
*/
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

/**
 * Get ip of the configuration
 * 
 * @return   ip of the configuration
*/
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

/**
 * Get gateway ip of the configuration
 * 
 * @return   gateway ip of the configuration
*/
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

/**
 * Get subnet mask of the configuration
 * 
 * @return   subnet mask of the configuration
*/
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

/**
 * Get ssid configuration
 * 
 * @return   ssid of the configuration
*/
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

/**
 * Get password configuration
 * 
 * @return   password of the configuration
*/
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

/**
 * print default parameter
*/
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

/**
 * print user parameter
*/
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