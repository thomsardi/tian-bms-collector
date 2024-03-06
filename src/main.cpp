#include <Arduino.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoOTA.h>
#include <ESP32httpUpdate.h>
#include <ModbusServerTCPasync.h>
#include <Preferences.h>
#include <nvs_flash.h>
#include <WiFiSetting.h>
#include <TianBMS.h>
#include <ModbusClientTCP.h>
#include <WiFiSave.h>
#include <Talis5Memory.h>
#include <Talis5JsonHandler.h>

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

#include "freertos/semphr.h"

// #define FZ_WITH_ASYNCSRV
#include <flashz-http.hpp>
// #include <flashz.hpp>

#include <map>
#include <vector>
#include "LittleFS.h"

const char *TAG = "ESP32-Tian-BMS-Collector";
#define FIRMWARE_VERSION 0.1

SemaphoreHandle_t write_mutex = NULL;
SemaphoreHandle_t read_mutex = NULL;

WiFiClient theClient;                          // Set up a client

FlashZhttp fz;
AsyncWebServer server(80);
ModbusClientTCP MB(theClient);

TianBMS reader;

Talis5Memory talis5Memory;
WiFiSave wifiSave;
WiFiSetting wifiSetting;

std::vector<uint8_t> slave;
std::map<int, TianBMSData>::iterator globalIterator;

unsigned long lastReconnectMillis;
unsigned long lastRequest;
unsigned long lastCleanup;
unsigned long lastQueueCheck;
unsigned long lastRestartMillis;
int reconnectInterval = 5000;
int internalLed = 2;

uint8_t slavePointer = 0;

bool isRestart = false;
bool isCleanup = false;
bool isSlaveChanged = false;
bool isScan = false;
uint8_t isScanFinished = false;
uint8_t emptyCount = 0;
uint8_t failCount = 0;

// put function declarations here:

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
    Serial.println("Wifi Connected");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info){
    Serial.print("Connected to ");
    Serial.println(WiFi.SSID());
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    // rmsInfo.ip = WiFi.localIP().toString();
    Serial.print("Subnet Mask: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("Gateway IP: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("DNS 1: ");
    Serial.println(WiFi.dnsIP(0));
    Serial.print("DNS 2: ");
    Serial.println(WiFi.dnsIP(1));
    Serial.print("Hostname: ");
    Serial.println(WiFi.getHostname());
    digitalWrite(internalLed, HIGH);
}

/**
 * Handle firmware upload
 * 
 * @brief it is used as an OTA firmware upload, pass the http received file into Update class which handle for flash writing
*/
void handleFirmwareUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
    Serial.println("Handle firmware upload");
    if(!index)
    {
        Serial.printf("Update Start: %s\n", filename.c_str());
        if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000))
        {
            Update.printError(Serial);
        }
    }
    if(!Update.hasError())
    {
        if(Update.write(data, len) != len)
        {
            Update.printError(Serial);
        }
    }
    if(final)
    {
        if(Update.end(true))
        {
            Serial.printf("Update Success: %uB\n", index+len);
        } 
        else 
        {
            Update.printError(Serial);
        }
    }
}

/**
 * Handle filesystem upload
 * 
 * @brief it is used as an OTA filesystem upload, pass the http received file into Update class which handle for flash writing
 * 
*/
void handleFilesystemUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
    Serial.println("Handle firmware upload");
    if(!index)
    {
        Serial.printf("Update Start: %s\n", filename.c_str());
        if(!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS))
        {
            Update.printError(Serial);
        }
    }
    if(!Update.hasError())
    {
        if(Update.write(data, len) != len)
        {
            Update.printError(Serial);
        }
    }
    if(final)
    {
        if(Update.end(true))
        {
            Serial.printf("Update Success: %uB\n", index+len);
        } 
        else 
        {
            Update.printError(Serial);
        }
    }
}

/**
 * Handle compressed upload with pigz and data format .zz
 * 
 * @brief   it is used as an OTA compressed upload with pigz , pass the http received file into Update class which handle for flash writing
 * 
 * @param[in]   request form field with key "img" with any value for firmware upload except "fs" for filesytem upload
 *                      "img" : "fs" will trigger filesystem upload, while "img" : "xxx" will trigger firmware upload with xxx is any value\
 * 
 * for other function parameter input please refer to AsyncWebServerRequest documentation
 *          
*/
void handleCompressedUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
    ESP_LOGI(TAG, "Handle compressed file upload\n");
    for (size_t i = 0; i < request->params(); i++)
    {
        ESP_LOGI(TAG, "%s:%s\n", request->getParam(i)->name().c_str(), request->getParam(i)->value().c_str());
    }
    fz.file_upload(request, filename, index, data, len, final);
}

void setupLittleFs()
{
  if(!LittleFS.begin()){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
}

// Define an onData handler function to receive the regular responses
// Arguments are the message plus a user-supplied token to identify the causing request
void handleData(ModbusMessage response, uint32_t token) 
{
    uint8_t functionCode = response.getFunctionCode();
    uint8_t serverId = response.getServerID();
    ESP_LOGI(TAG, "Server ID : %d\n", serverId);
    
    if (functionCode == READ_HOLD_REGISTER || functionCode == READ_INPUT_REGISTER)
    {
        uint8_t regCount;
        uint16_t buffer[64];
        uint8_t index = response.get(2, regCount);
        regCount /= 2;
        for (size_t i = 0; i < regCount; i++)
        {
            uint16_t content;
            index = response.get(index, content);
            buffer[i] = content;
        }
        if(xSemaphoreTake(write_mutex, 0))
        {
            ESP_LOGI(TAG, "mutex on update");
            ESP_LOGI(TAG, "write mutex taken");
            reader.update(serverId, token, buffer, regCount);
            ESP_LOGI(TAG, "write_mutex given");
            xSemaphoreGive(write_mutex);
        }
    }
    
    // Serial.printf("Response: serverID=%d, FC=%d, Token=%08X, length=%d:\n", response.getServerID(), response.getFunctionCode(), token, response.size());
    // for (auto& byte : response) {
    //     Serial.printf("%02X ", byte);
    // }
    // Serial.println("");
}

// Define an onError handler function to receive error responses
// Arguments are the error code returned and a user-supplied token to identify the causing request
void handleError(Error error, uint32_t token) 
{
    // ModbusError wraps the error code and provides a readable error message for it
    ModbusError me(error);
    Serial.printf("Error response: %02X - %s\n", (int)me, (const char *)me);
    if(xSemaphoreTake(write_mutex, 0))
    {
        ESP_LOGI(TAG, "mutex on error update");
        reader.updateOnError(token);
        xSemaphoreGive(write_mutex);
    }
}

void setup() {
  // put your setup code here, to run once:
    
    pinMode(internalLed, OUTPUT);
    Serial.begin(115200); 
    Serial.setDebugOutput(true);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    write_mutex = xSemaphoreCreateMutex();
    if(write_mutex != NULL )
    {
        ESP_LOGI(TAG, "Successfully create write mutex");
    }
    read_mutex = xSemaphoreCreateMutex();
    if(read_mutex != NULL )
    {
        ESP_LOGI(TAG, "Successfully create read mutex");
    }

    setupLittleFs();
    WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
    WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);

    wifiSave.begin("wifi_param");
    // wifiSave.reset();
    // run only for first time
    // wifiSave.setMode(mode_type::STATION);
    // wifiSave.setServer(server_type::STATIC);
    // wifiSave.setSsid("RnD_Sundaya");
    // wifiSave.setPassword("sundaya22");
    // wifiSave.setIp("192.168.2.144");
    // wifiSave.setGateway("192.168.2.1");
    // wifiSave.setSubnet("255.255.255.0");
    // wifiSave.save();

    talis5Memory.begin("talis5_param");
    // talis5Memory.setModbusTargetIp("192.168.2.113");
    // talis5Memory.setModbusPort(502);
    // uint8_t list[7] = {1,2,5,7,10,12,32};
    // talis5Memory.setSlave(list, 7);
    // talis5Memory.save();

    ESP_LOGI(TAG, "Number of slave : %d\n", talis5Memory.getSlaveSize());

    slave.reserve(255);
    slave.resize(talis5Memory.getSlaveSize());
    talis5Memory.getSlave(slave.data(), talis5Memory.getSlaveSize());

    ESP_LOGI(TAG, "number of stored address : %d\n", talis5Memory.getSlaveSize());

    for (size_t i = 0; i < talis5Memory.getSlaveSize(); i++)
    {
        ESP_LOGI(TAG, "address List : %d\n", slave.at(i));
    }


  	WifiParams wifiParams;
    wifiParams.mode = wifiSave.getMode(); // 1 to set as AP, 2 to set as STATION, 3 to set AP + STATION
    wifiParams.params.server = wifiSave.getServer(); // 1 to set into static IP, 2 to set into dhcp
    strcpy(wifiParams.params.ssid.data(), wifiSave.getSsid().c_str());
    strcpy(wifiParams.params.pass.data(), wifiSave.getPassword().c_str());
    strcpy(wifiParams.params.ip.data(), wifiSave.getIp().c_str());
    strcpy(wifiParams.params.gateway.data(), wifiSave.getGateway().c_str());
    strcpy(wifiParams.params.subnet.data(), wifiSave.getSubnet().c_str());

    wifiParams.softApParams.server = server_type::STATIC;
    String ssidName = "esp32-" + WiFi.macAddress();
    ssidName.replace(":", "");
    strcpy(wifiParams.softApParams.ssid.data(), ssidName.c_str());
    strcpy(wifiParams.softApParams.pass.data(), "esp32-default");
    strcpy(wifiParams.softApParams.ip.data(), "192.168.4.1");
    strcpy(wifiParams.softApParams.gateway.data(), "192.168.4.1");
    strcpy(wifiParams.softApParams.subnet.data(), "255.255.255.0");

    wifiSetting.begin(wifiParams);

    int timeout = 0;
    
    if (wifiSetting.getMode() == mode_type::STATION || wifiSetting.getMode() == mode_type::AP_STATION)
    {
        while (WiFi.status() != WL_CONNECTED)
        {
            if (timeout >= 10)
            {
                Serial.println("Failed to connect into " + wifiSetting.getSsid());
                break;
            }
            Serial.print(".");
            delay(500);
            timeout++;
        }
    }
    
    if (wifiSetting.getMode() == mode_type::AP)
    {
        Serial.println("AP Connected");
        Serial.print("SSID : ");
        Serial.println(WiFi.softAPSSID());
        Serial.print("IP address: ");
        Serial.println(WiFi.softAPIP().toString());
        Serial.print("Subnet Mask: ");
        Serial.println(WiFi.softAPSubnetMask().toString());
        Serial.print("Hostname: ");
        Serial.println(WiFi.softAPgetHostname());
        digitalWrite(internalLed, HIGH);
    }
    else if (wifiSetting.getMode() == mode_type::AP_STATION)
    {
        Serial.println("AP SSID : ");
        Serial.println(WiFi.softAPSSID());
        Serial.print("IP address: ");
        Serial.println(WiFi.softAPIP().toString());
        Serial.print("Hostname: ");
        Serial.println(WiFi.softAPgetHostname());
    }

    if (timeout >= 10)
    {
        Serial.println("WiFi Not Connected");
        digitalWrite(internalLed, LOW);
    }

    MB.onDataHandler(&handleData);
    MB.onErrorHandler(&handleError);
    MB.setTimeout(2000, 200);
    MB.begin();
    IPAddress ip;
    if (ip.fromString(talis5Memory.getModbusTargetIp()))
    {
        MB.setTarget(ip, talis5Memory.getModbusPort());
    }
    // MB.setTarget(IPAddress(192, 168, 2, 113), 502);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
		request->send(LittleFS, "/index.html", "text/html");
	});

    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/update.html", "text/html");
    });

    server.on("/info", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/info.html", "text/html");
    });

    server.on("/assets/progressUpload.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/assets/progressUpload.js", "application/javascript");
    });

    server.on("/assets/function_without_ajax.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/assets/function_without_ajax.js", "application/javascript");
    });

    server.on("/assets/function_info_page.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/assets/function_info_page.js", "application/javascript");
    });

    server.on("/assets/index.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/assets/index.css", "text/css");
    });

    server.on("/assets/info.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/assets/info.css", "text/css");
    });

    server.on("/assets/update_styles.css", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/assets/update_styles.css", "text/css");
    });

    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/assets/favicon.ico", "image/x-icon");
    });

    server.on("/api/get-data", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        std::map<int, TianBMSData> bufferData;
        reader.getCloneTianBMSData(bufferData);
        ESP_LOGI(TAG, "buffer data size : %d\n", bufferData.size());
        AsyncWebServerResponse *response = request->beginChunkedResponse("application/json", [bufferData](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
            //Write up to "maxLen" bytes into "buffer" and return the amount written.
            //index equals the amount of bytes that have been already sent
            //You will be asked for more data until 0 is returned
            //Keep in mind that you can not delay or yield waiting for more data!
            static auto it = bufferData.begin();
            static bool isTerminated = false;
            static bool isOverflow = false;
            static String leftOverInput = "";
            TianBMSJsonManager jman;
            int len = 0;
            // This block will execute when the buffer is not enough to store json
            if (isOverflow)
            {
                if (leftOverInput.length() > maxLen)
                {
                    len = maxLen;
                    memcpy(buffer, leftOverInput.begin(), len);
                    leftOverInput = leftOverInput.substring(len);
                }
                else
                {
                    isOverflow = false;
                    len = leftOverInput.length();
                    memcpy(buffer, leftOverInput.begin(), len);
                    leftOverInput = "";
                }
                return len;
            }

            // This block is crucial to terminate the browser request
            if (isTerminated)
            {
                isTerminated = false;
                it = reader.getTianBMSData().begin();
                return len;
            }

            // This block will send the json formatted data
            if (index == 0) // send the first open part of the json "{"data" : ["
            {
                it = bufferData.begin();
                String startPart = "{\"data\":[";
                for (size_t i = 0; i < startPart.length(); i++)
                {
                    buffer[i] = startPart.charAt(i);
                    len++;
                }
                return len;
            }
            else
            {
                if (it == bufferData.end()) // send the last end part of the json "]}"
                {
                    String stopPart = "]}";
                    String input = stopPart;
                    isTerminated = true;
                    for (size_t i = 0; i < input.length(); i++)
                    {
                        buffer[i] = input.charAt(i);
                        len++;
                    }
                }
                else // send the bms data as formatted json document
                {
                    String input = jman.buildData((*it).second);
                    it++;
                    if (it != bufferData.end())
                    {
                        input += ",";
                    }
                    if (input.length() < maxLen)
                    {
                        len = input.length();
                    }
                    else // detect if the converted json document size larger than buffer, this will set the overflow flag
                    {
                        len = maxLen;
                        isOverflow = true;
                        leftOverInput = input.substring(len);
                    }
                    memcpy(buffer, input.begin(), len); //copy the input into the buffer with length of len
                }
                return len;
            }
        });

        response->addHeader("Server","ESP Async Web Server");
        request->send(response);

        });

    server.on("/api/get-device-info", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        String output;
        StaticJsonDocument<256> doc;
        doc["firmware_version"] = FIRMWARE_VERSION;
        if (wifiSave.getMode() == 1)
        {
            doc["mode"] = "AP";
            doc["device_ip"] = WiFi.softAPIP().toString(); 
            doc["ssid"] = WiFi.softAPSSID();
            doc["gateway"] = "192.168.4.1";
            doc["subnet"] = "255.255.255.0";
            
        }
        else
        {
            switch (wifiSave.getMode())
            {
            case 2:
                /* code */
                doc["mode"] = "STATION";
                break;
            case 3:
                doc["mode"] = "AP + STATION";
                break;
            default:
                break;
            }
            doc["device_ip"] = WiFi.localIP().toString();
            doc["ssid"] = WiFi.SSID();
            doc["gateway"] = WiFi.gatewayIP().toString();
            doc["subnet"] = WiFi.subnetMask().toString();
        }
        
        doc["mac_address"] = WiFi.macAddress();

        if (wifiSave.getServer() == 1)
        {
            doc["server_mode"] = "STATIC";
        }
        else
        {
            doc["server_mode"] = "DHCP";
        }
        serializeJson(doc, output);
        request->send(200, "application/json", output); });

    server.on("/api/get-modbus-info", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        StaticJsonDocument<768> doc;
        String output;
        doc["modbus_ip"] = talis5Memory.getModbusTargetIp();
        doc["port"] = talis5Memory.getModbusPort();
        std::vector<uint8_t> buff;
        buff.reserve(255);
        buff.resize(talis5Memory.getSlaveSize());
        size_t slaveNum = talis5Memory.getSlave(buff.data(), talis5Memory.getSlaveSize());
        JsonArray slave_list = doc.createNestedArray("slave_list");
        for (size_t i = 0; i < buff.size(); i++)
        {
            slave_list.add(buff.at(i));
        }

        serializeJson(doc, output);
        request->send(200, "application/json", output); });

    server.on("/api/get-active-slave", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        StaticJsonDocument<768> doc;
        String output;
        doc["address_status"] = isScanFinished;
        JsonArray slave_list = doc.createNestedArray("slave_list");
        std::map<int, TianBMSData>::iterator it;
        for (it = reader.getTianBMSData().begin(); it != reader.getTianBMSData().end(); it++)
        {
            slave_list.add((*it).first);
        }

        serializeJson(doc, output);

        request->send(200, "application/json", output); });

    server.on("/api/update-firmware", HTTP_POST, [](AsyncWebServerRequest *request){
        String output;
        int code = 200;
        StaticJsonDocument<16> doc;
        doc["status"] = code;
        serializeJson(doc, output);
        request->send(200, "text/plain", "File has been uploaded successfully.");
        isRestart = true;
        // lastTime = millis();
    }, handleFirmwareUpload);

    server.on("/api/update-compressed-firmware", HTTP_POST, [](AsyncWebServerRequest *request){
        String output;
        int code = 200;
        StaticJsonDocument<16> doc;
        doc["status"] = code;
        serializeJson(doc, output);
        request->send(200, "text/plain", "File has been uploaded successfully.");
        isRestart = true;
    }, handleCompressedUpload);

    server.on("/api/update-filesystem", HTTP_POST, [](AsyncWebServerRequest *request){
        String output;
        int code = 200;
        StaticJsonDocument<16> doc;
        doc["status"] = code;
        serializeJson(doc, output);
        request->send(200, "text/plain", "File has been uploaded successfully.");
        isRestart = true;
    }, handleFilesystemUpload);

    server.on("/api/update-compressed-filesystem", HTTP_POST, [](AsyncWebServerRequest *request){
        String output;
        int code = 200;
        StaticJsonDocument<16> doc;
        doc["status"] = code;
        serializeJson(doc, output);
        request->send(200, "text/plain", "File has been uploaded successfully.");
        isRestart = true;
    }, handleCompressedUpload);

    AsyncCallbackJsonWebHandler *setSlaveHandler = new AsyncCallbackJsonWebHandler("/api/set-slave", [](AsyncWebServerRequest *request, JsonVariant &json)
    {
        ESP_LOGI(TAG, "----------------set slave----------------");
        ESP_LOGI(TAG, "%s\n", json.as<String>().c_str());
        Talis5JsonHandler handler;
        int status = 400;
        std::vector<uint8_t> buff;
        size_t len = handler.parseSetSlaveJson(json, buff);
        if (len != 0)
        {
            status = 200;
            talis5Memory.setSlave(buff.data(), len);
            isSlaveChanged = true;
            lastQueueCheck = millis();
        }
        String response = handler.buildJsonResponse(status);
        request->send(status, "application/json", response);
        return;
    });

    AsyncCallbackJsonWebHandler *setScanHandler = new AsyncCallbackJsonWebHandler("/api/scan", [](AsyncWebServerRequest *request, JsonVariant &json)
    {
        Talis5JsonHandler handler;
        int status = 400;
        if (handler.parseModbusScan(json))
        {
            status = 200;
            isScan = true;
        }
        request->send(status, "application/json", handler.buildJsonResponse(status));
        });

    AsyncCallbackJsonWebHandler *setModbus = new AsyncCallbackJsonWebHandler("/api/set-modbus", [](AsyncWebServerRequest *request, JsonVariant &json)
    {
        ESP_LOGI(TAG, "----------------set modbus----------------");
        Talis5JsonHandler handler;
        Talis5ParameterData param;
        int status = 400;
        if (handler.parseSetModbus(json, param))
        {
            status = 200;
            talis5Memory.setModbusTargetIp(param.modbusTargetIp);
            talis5Memory.setModbusPort(param.modbusPort);
            talis5Memory.save();
            IPAddress ip;
            if (ip.fromString(param.modbusTargetIp))
            {
                MB.setTarget(ip, param.modbusPort);
            }
        }
        request->send(status, "application/json", handler.buildJsonResponse(status));
        });

    AsyncCallbackJsonWebHandler *restartHandler = new AsyncCallbackJsonWebHandler("/api/restart", [](AsyncWebServerRequest *request, JsonVariant &json)
    {
        Talis5JsonHandler handler;
        int status = 400;
        if (handler.parseRestart(json))
        {
            status = 200;
            isRestart = true;
        }
        request->send(status, "application/json", handler.buildJsonResponse(status));
        });

    AsyncCallbackJsonWebHandler *setNetwork = new AsyncCallbackJsonWebHandler("/api/set-network", [](AsyncWebServerRequest *request, JsonVariant &json)
    {
        ESP_LOGI(TAG, "----------------set network----------------");
        int status = 400;
        WifiParameterData wifiParameterData;
        Talis5JsonHandler parser;
        if (parser.parseSetNetwork(json, wifiParameterData))
        {
            status = 200;
            ESP_LOGI(TAG, "mode : %d\n", wifiParameterData.mode);
            ESP_LOGI(TAG, "server : %d\n", wifiParameterData.server);
            ESP_LOGI(TAG, "ip : %s\n", wifiParameterData.ip.c_str());
            ESP_LOGI(TAG, "gateway : %s\n", wifiParameterData.gateway.c_str());
            ESP_LOGI(TAG, "subnet : %s\n", wifiParameterData.subnet.c_str());
            ESP_LOGI(TAG, "ssid : %s\n", wifiParameterData.ssid.c_str());
            ESP_LOGI(TAG, "password : %s\n", wifiParameterData.password.c_str());
            wifiSave.setMode(wifiParameterData.mode);
            wifiSave.setServer(wifiParameterData.server);
            wifiSave.setIp(wifiParameterData.ip);
            wifiSave.setGateway(wifiParameterData.gateway);
            wifiSave.setSubnet(wifiParameterData.subnet);
            wifiSave.setSsid(wifiParameterData.ssid);
            wifiSave.setPassword(wifiParameterData.password);
            wifiSave.save();
        }
        request->send(status, "application/json", parser.buildJsonResponse(status));
    });

    AsyncCallbackJsonWebHandler *setFactoryReset = new AsyncCallbackJsonWebHandler("/api/freset", [](AsyncWebServerRequest *request, JsonVariant &json)
    {
        Talis5JsonHandler handler;
        int status = 400;
        if (handler.parseFactoryReset(json))
        {
            status = 200;
            talis5Memory.reset();
            wifiSave.reset();
            isRestart = true;
        }
        request->send(status, "application/json", handler.buildJsonResponse(status));
    });

    server.addHandler(setNetwork);
    server.addHandler(setSlaveHandler);
    server.addHandler(setScanHandler);
    server.addHandler(setModbus);
    server.addHandler(restartHandler);
    server.addHandler(setFactoryReset);
    server.onNotFound([](AsyncWebServerRequest *request) {
        if (request->method() == HTTP_OPTIONS) {
            request->send(200);
        } else {
            request->send(404);
        }
    });
    server.begin();
    globalIterator = reader.getTianBMSData().begin();
    lastRequest = millis();
    lastCleanup = millis();
}

void loop() {
  // put your main code here, to run repeatedly:
  	// ESP_LOGI(TAG, "ULULULULU");
    
    if (wifiSetting.getMode() == mode_type::STATION || wifiSetting.getMode() == mode_type::AP_STATION)
	{
		if ((WiFi.status() != WL_CONNECTED) && (millis() - lastReconnectMillis >= reconnectInterval)) {
			digitalWrite(internalLed, LOW);
			ESP_LOGI(TAG, "===============Reconnecting to WiFi...========================\n");
			if (WiFi.reconnect())
			{
				lastReconnectMillis = millis();
			}
		}
	}

    /**
     * TO DO : This block is used to clean up obsolete data to free up space, but still causing crash
    */
    // if (isScanFinished)
    // {
    //     if (millis() - lastCleanup > 1000)
    //     {
    //         isCleanup = true;
    //         if (MB.pendingRequests() == 0) // check if there is still pending request on queue, wait until it is empty
    //         {
    //             reader.cleanUp(); // perform cleanup
    //             ESP_LOGI(TAG, "clean up");
    //             isCleanup = false;
    //             std::map<int, TianBMSData>::iterator it = reader.getTianBMSData().begin();
    //             if (it == reader.getTianBMSData().end()) // check if data is empty
    //             {
    //                 emptyCount++;
    //                 if (emptyCount > 3) // if it is empty during > 3 times check, then perform address scan again
    //                 {
    //                     isScanFinished = false;
    //                     emptyCount = 0;
    //                 }
    //             }
    //             lastCleanup = millis();
    //         }
    //     }
    // }
    // else
    // {
    //     lastCleanup = millis();
    // }

    if(isSlaveChanged || isScan)
    {
        if (MB.pendingRequests() == 0)
        {
            if (millis() - lastQueueCheck > 3000)
            {
                if (xSemaphoreTake(read_mutex, portMAX_DELAY))
                {
                    if (xSemaphoreTake(write_mutex, portMAX_DELAY))
                    {
                        // check if the signal is coming from slave changed flag, the new setting need to be written, if not it is coming from rescan flag
                        if (isSlaveChanged) 
                        {
                            ESP_LOGI(TAG, "SAVE PARAMETER AND CLEAR");
                            talis5Memory.save();
                            slave.resize(talis5Memory.getSlaveSize());
                            talis5Memory.getSlave(slave.data(), talis5Memory.getSlaveSize());
                        }
                        else
                        {
                            ESP_LOGI(TAG, "RESCAN");
                        }
                        isScanFinished = false;
                        isSlaveChanged = false;
                        isScan = false;
                        reader.clearData();
                        xSemaphoreGive(write_mutex);
                    }
                    xSemaphoreGive(read_mutex);
                }
                lastQueueCheck = millis();
            }
        }
        else
        {
            lastQueueCheck = millis();
        }
    }

    if (isScanFinished)
    {
        /**
         * This block will push the request to queue based on detected slave
        */
        // if (!isCleanup) // this flag is to detect if it's time to do cleanup, then pause the .addRequest
        // { 
        if (!isSlaveChanged && !isScan) // if it is not scan or not slave changed, do normal polling
        {
            if (globalIterator != reader.getTianBMSData().end())
            {
                if (millis() - lastRequest > 500)
                {
                    Error err = MB.addRequest(reader.getToken((*globalIterator).first, TianBMSUtils::REQUEST_DATA), (*globalIterator).first, READ_INPUT_REGISTER, 4096, 34);
                    if (err!=SUCCESS) {
                        ModbusError e(err);
                        Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
                    }
                    lastRequest = millis();
                    globalIterator++; // increment the iterator to the next element
                }
            }
            else
            {
                globalIterator = reader.getTianBMSData().begin(); // get the iterator of the first element of data
            }
        }
        // }
    }
    else
    {
        /**
         * This block is to do address(slave) scan, start from slave 1 to 16
        */
        if (millis() - lastRequest > 500)
        {
            ESP_LOGI(TAG, "Slave pointer : %d\n", slavePointer);
            ESP_LOGI(TAG, "Slave address : %d\n", slave.at(slavePointer));
            Error err = MB.addRequest(reader.getToken(slave.at(slavePointer), TianBMSUtils::REQUEST_SCAN), slave.at(slavePointer), READ_INPUT_REGISTER, 4096, 1);
            if (err!=SUCCESS) {
                ModbusError e(err);
                Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
            }
            slavePointer++;
            if (slavePointer >= slave.size())
            {
                slavePointer = 0;
                isScanFinished = 1;
                globalIterator = reader.getTianBMSData().begin();
            }
            lastRequest = millis();
        }
    }
    
    
    

    // ESP_LOGI(TAG, "PCB Code : %s\n", tianBMS.getPcbBarcode().c_str());
	// if (factoryReset)
	// {
	// 	talisMemory.reset();
	// 	ESP_LOGI(TAG, "Factory Reset");
	// 	rmsRestartCommand.restart = true;
	// 	factoryReset = false;
	// }

	if (isRestart)
	{
		isRestart = false;
		digitalWrite(internalLed, LOW);
		delay(100);
		ESP.restart();
	}
  
}