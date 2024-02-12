#include <Arduino.h>
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

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

// #define FZ_WITH_ASYNCSRV
#include <flashz-http.hpp>
// #include <flashz.hpp>

#include <map>
#include "LittleFS.h"

const char *TAG = "ESP32-Tian-BMS-Collector";

WiFiClient theClient;                          // Set up a client

FlashZhttp fz;
AsyncWebServer server(80);
ModbusClientTCP MB(theClient);

TianBMS reader;

WiFiSetting wifiSetting;

unsigned long lastReconnectMillis;
unsigned long lastRequest;
int reconnectInterval = 5000;
int internalLed = 2;

int num = 1;

bool isRestart = false;

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
    
    if (functionCode == READ_HOLD_REGISTER)
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
        ESP_LOGI(TAG, "update");
        reader.update(serverId, token, buffer, regCount);
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
}

void setup() {
  // put your setup code here, to run once:

    pinMode(internalLed, OUTPUT);
    Serial.begin(115200); 
    Serial.setDebugOutput(true);
    esp_log_level_set(TAG, ESP_LOG_INFO);
    setupLittleFs();
    WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
    WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);

  	WifiParams wifiParams;
    wifiParams.mode = mode_type::AP_STATION;
    wifiParams.params.server = server_type::DHCP; // 1 to set into static IP, 2 to set into dhcp
    strcpy(wifiParams.params.ssid.data(), "RnD_Sundaya");
    strcpy(wifiParams.params.pass.data(), "sundaya22");
    strcpy(wifiParams.params.ip.data(), "192.168.2.251");
    strcpy(wifiParams.params.gateway.data(), "192.168.2.1");
    strcpy(wifiParams.params.subnet.data(), "255.255.255.0");

    wifiParams.softApParams.server = server_type::STATIC;
    strcpy(wifiParams.softApParams.ssid.data(), "ESP32-Tian-BMS");
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
    MB.setTarget(IPAddress(192, 168, 2, 113), 502);

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

    server.on("/get-test-data", HTTP_GET, [](AsyncWebServerRequest *request)
    {
        AsyncWebServerResponse *response = request->beginChunkedResponse("text/plain", [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
            //Write up to "maxLen" bytes into "buffer" and return the amount written.
            //index equals the amount of bytes that have been already sent
            //You will be asked for more data until 0 is returned
            //Keep in mind that you can not delay or yield waiting for more data!
            static bool isTerminated = false;
            static bool isOverflow = false;
            static auto it = reader.getTianBMSData().begin();
            static String leftOverInput = "";
            TianBMSJsonManager jman;
            int len = 0;
            
            // This block will execute when the buffer is not enough to store json
            if (isOverflow)
            {
                if (leftOverInput.length() > maxLen)
                {
                    len = maxLen;
                    leftOverInput = leftOverInput.substring(len);
                }
                else
                {
                    isOverflow = false;
                    len = leftOverInput.length();
                    leftOverInput = "";
                }
                memcpy(buffer, leftOverInput.begin(), len);
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
                if (it == reader.getTianBMSData().end()) // send the last end part of the json "]}"
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
                    if (it != reader.getTianBMSData().end())
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
        request->send(response); });

    // server.on("/get-cms-data", HTTP_GET, [](AsyncWebServerRequest *request)
    // {
    //     // String jsonOutput = jsonManager.buildJsonData(request, cellData, cellDataArrSize);
    //     String jsonOutput = jsonManager.buildJsonData(request, packedData);
    //     request->send(200, "application/json", jsonOutput); });

    // server.on("/get-addressing-status", HTTP_GET, [](AsyncWebServerRequest *request)
    // {
    //     AddressingStatus addressingStatus;
    //     addressingStatus.numOfDevice = addressList.size();
    //     Serial.println("Number of Device : " + String(addressingStatus.numOfDevice));
    //     for (int i = 0; i < addressList.size(); i++)
    //     {
    //         addressingStatus.deviceAddressList[i] = addressList.at(i);
    //         Serial.println(addressingStatus.deviceAddressList[i]);
    //     }
    //     addressingStatus.status = isAddressed;
    //     Serial.println("Addressing Completed Flag : " + String(addressingStatus.status));
    //     String jsonOutput = jsonManager.buildJsonAddressingStatus(addressingStatus, addressList.size());
    //     request->send(200, "application/json", jsonOutput); });

    server.on("/update-firmware", HTTP_POST, [](AsyncWebServerRequest *request){
        String output;
        int code = 200;
        StaticJsonDocument<16> doc;
        doc["status"] = code;
        serializeJson(doc, output);
        request->send(200, "text/plain", "File has been uploaded successfully.");
        isRestart = true;
        // lastTime = millis();
    }, handleFirmwareUpload);

    server.on("/update-compressed-firmware", HTTP_POST, [](AsyncWebServerRequest *request){
        String output;
        int code = 200;
        StaticJsonDocument<16> doc;
        doc["status"] = code;
        serializeJson(doc, output);
        request->send(200, "text/plain", "File has been uploaded successfully.");
        isRestart = true;
    }, handleCompressedUpload);

    server.on("/update-filesystem", HTTP_POST, [](AsyncWebServerRequest *request){
        String output;
        int code = 200;
        StaticJsonDocument<16> doc;
        doc["status"] = code;
        serializeJson(doc, output);
        request->send(200, "text/plain", "File has been uploaded successfully.");
        isRestart = true;
    }, handleFilesystemUpload);

    server.on("/update-compressed-filesystem", HTTP_POST, [](AsyncWebServerRequest *request){
        String output;
        int code = 200;
        StaticJsonDocument<16> doc;
        doc["status"] = code;
        serializeJson(doc, output);
        request->send(200, "text/plain", "File has been uploaded successfully.");
        isRestart = true;
    }, handleCompressedUpload);

    // AsyncCallbackJsonWebHandler *setAddressHandler = new AsyncCallbackJsonWebHandler("/set-addressing", [](AsyncWebServerRequest *request, JsonVariant &json)
    // {
    //     String input = json.as<String>();
    //     StaticJsonDocument<16> doc;
    //     String response;
    //     int status = jsonManager.jsonAddressingCommandParser(input.c_str());
    //     doc["status"] = status;
    //     serializeJson(doc, response);
    //     if (status >= 0)
    //     {
    //         if (status)
    //         {
    //             isAddressing = true;
    //         }
    //         else
    //         {
    //             isAddressing = false;
    //         }
    //         request->send(200, "application/json", response);
    //     }
    //     else
    //     {
    //         request->send(400, "application/json", response);
    //     }
    //     });

    // AsyncCallbackJsonWebHandler *setDataCollectionHandler = new AsyncCallbackJsonWebHandler("/set-data-collection", [](AsyncWebServerRequest *request, JsonVariant &json)
    // {
    //     String input = json.as<String>();
    //     StaticJsonDocument<16> doc;
    //     String response;
    //     int status = jsonManager.jsonDataCollectionCommandParser(input.c_str());
    //     doc["status"] = status;
    //     serializeJson(doc, response);
    //     dataCollectionCommand.exec = status;
    //     request->send(200, "application/json", response); });

    // AsyncCallbackJsonWebHandler *restartRMSHandler = new AsyncCallbackJsonWebHandler("/restart", [](AsyncWebServerRequest *request, JsonVariant &json)
    // {
    //     String input = json.as<String>();
    //     StaticJsonDocument<16> doc;
    //     String response;
    //     int status = jsonManager.jsonRMSRestartParser(input.c_str());
    //     doc["status"] = status;
    //     serializeJson(doc, response);
    //     if (status >= 0)
    //     {
    //         if (status > 0)
    //         {
    //             rmsRestartCommand.restart = true;
    //         }
    //         request->send(200, "application/json", response);
    //     }
    //     else
    //     {
    //         request->send(400, "application/json", response);
    //     }
    //     });

    // AsyncCallbackJsonWebHandler *setFrameHandler = new AsyncCallbackJsonWebHandler("/set-frame", [](AsyncWebServerRequest *request, JsonVariant &json)
    // {
    //     String input = json.as<String>(); 
    //     StaticJsonDocument<16> doc;
    //     String response;
    //     SlaveWrite slaveWrite;
    //     int status = jsonManager.jsonCMSFrameParser(input.c_str(), slaveWrite);
    //     doc["status"] = status;
    //     serializeJson(doc, response);
    //     if (status >= 0)
    //     {
    //         if (status > 0)
    //         {
    //             TalisRS485TxMessage txMsg;
    //             txMsg.token = TalisRS485::RequestType::CMSFRAMEWRITE;
    //             txMsg.id = slaveWrite.bid;
    //             TalisRS485Message::createCMSFrameWriteIdRequest(txMsg, slaveWrite.content);
    //             userCommand.push_back(txMsg);
    //         }
    //         request->send(200, "application/json", response);
    //     }
    //     else
    //     {
    //         request->send(400, "application/json", response);
    //     }
    //     });

    // AsyncCallbackJsonWebHandler *setNetwork = new AsyncCallbackJsonWebHandler("/set-network", [](AsyncWebServerRequest *request, JsonVariant &json)
    // {
    //     StaticJsonDocument<16> doc;
    //     String response;
    //     NetworkSetting setting = jsonManager.parseNetworkSetting(json);
    //     doc["status"] = setting.flag;
    //     serializeJson(doc, response);
    //     if (setting.flag > 0)
    //     {
    //         Serial.println("Set network");
    //         size_t resultLength = Utilities::toDoubleChar(setting.ssid, ssidArr, 8, true);
    //         // Serial.println(resultLength);
    //         resultLength = Utilities::toDoubleChar(setting.pass, passArr, 8, true);
    //         // Serial.println(resultLength);
    //         IPAddress ip;
    //         IPAddress gateway;
    //         IPAddress subnet;
    //         ip.fromString(setting.ip);
    //         gateway.fromString(setting.gateway);
    //         subnet.fromString(setting.subnet);
    //         for (size_t i = 0; i < 4; i++)
    //         {
    //             ipOctet[i] = ip[i];
    //             // Serial.println(ipOctet[i]);
    //             gatewayOctet[i] = gateway[i];
    //             // Serial.println(gatewayOctet[i]);
    //             subnetOctet[i] = subnet[i];
    //             // Serial.println(subnetOctet[i]);
    //         }

    //         gServerType = wifiSetting.getStationServer();
    //         gMode = wifiSetting.getMode();
            
    //         talisMemory.setSsid(setting.ssid.c_str());
    //         talisMemory.setPass(setting.pass.c_str());
    //         talisMemory.setIp(setting.ip.c_str());
    //         talisMemory.setGateway(setting.gateway.c_str());
    //         talisMemory.setSubnet(setting.subnet.c_str());
    //         talisMemory.setServer(setting.server);
    //         talisMemory.setMode(setting.mode);

    //         request->send(200, "application/json", response);
    //     }
    //     else
    //     {
    //         request->send(400, "application/json", response);
    //     }
    // });

    // AsyncCallbackJsonWebHandler *setFactoryReset = new AsyncCallbackJsonWebHandler("/set-factory-reset", [](AsyncWebServerRequest *request, JsonVariant &json)
    // {
    //     StaticJsonDocument<16> doc;
    //     String response;
    //     int status = jsonManager.parseFactoryReset(json);
    //     doc["status"] = status;
    //     serializeJson(doc, response);
    //     if (status == 1 || status == 0)
    //     {
    //         if (status)
    //         {
    //             factoryReset = 1;
    //         }
    //         request->send(200, "application/json", response);
    //     }
    //     else
    //     {
    //         request->send(400, "application/json", response);
    //     }
        
    // });

    // server.addHandler(setBalancingHandler);
    // server.addHandler(setAddressHandler);
    // server.addHandler(setAlarmHandler);
    // server.addHandler(setDataCollectionHandler);
    // server.addHandler(setAlarmParamHandler);
    // server.addHandler(setHardwareAlarmHandler);
    // server.addHandler(setSleepHandler);
    // server.addHandler(setWakeupHandler);
    // server.addHandler(setRmsCodeHandler);
    // server.addHandler(setRackSnHandler);
    // server.addHandler(setFrameHandler);
    // server.addHandler(setCmsCodeHandler);
    // server.addHandler(setBaseCodeHandler);
    // server.addHandler(setMcuCodeHandler);
    // server.addHandler(setSiteLocationHandler);
    // server.addHandler(setLedHandler);
    // server.addHandler(restartCMSHandler);
    // server.addHandler(restartRMSHandler);
    // server.addHandler(setNetwork);
    // server.addHandler(setFactoryReset);
    // server.addHandler(setSoc);
    server.begin();
    lastRequest = millis();
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

    if (millis() - lastRequest > 500)
    {
        Error err = MB.addRequest(reader.getToken(1, TianBMSUtils::REQUEST_DATA), 1, READ_HOLD_REGISTER, 4096, 43);
        if (err!=SUCCESS) {
            ModbusError e(err);
            Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
        }
        lastRequest = millis();
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