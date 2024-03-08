#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <nvs_flash.h>
#include <TianBMS.h>
#include <ModbusClientRTU.h>
#include <Talis5Memory.h>
#include <Talis5JsonHandler.h>


#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

#include "freertos/semphr.h"

// #define FZ_WITH_ASYNCSRV
#define FZ_NOHTTPCLIENT
#include <flashz-http.hpp>
#include <flashz.hpp>

#define NO_OTA_NETWORK
// #define OTETHERNET
#include <ArduinoOTA.h>

#include <WebServer.h>

#include <map>
#include <vector>
#include "LittleFS.h"
#include "defines.h"

const char *TAG = "ESP32-Tian-BMS-Collector";
#define FIRMWARE_VERSION 0.1

SemaphoreHandle_t write_mutex = NULL;
SemaphoreHandle_t read_mutex = NULL;
TaskHandle_t httpTaskHandle;

// AsyncWebServer server(80);

EthernetWebServer server(80);
ethernetHTTPUpload upload;

ModbusClientRTU MB;

TianBMS reader;

Talis5Memory talis5Memory;
EthernetSave ethernetSave;
// WiFiSave wifiSave;
// WiFiSetting wifiSetting;

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

bool isFlashFailed = false;

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
 * get content type of filename
 * 
 * @brief this will get the type of filename to set the content-type of header response
 * 
 * @return  content-type
*/
String getContentType(const String& filename)
{
  if (server.hasArg("download"))
  {
    return "application/octet-stream";
  }
  else if (filename.endsWith(".htm"))
  {
    return "text/html";
  }
  else if (filename.endsWith(".html"))
  {
    return "text/html";
  }
  else if (filename.endsWith(".css"))
  {
    return "text/css";
  }
  else if (filename.endsWith(".js"))
  {
    return "application/javascript";
  }
  else if (filename.endsWith(".png"))
  {
    return "image/png";
  }
  else if (filename.endsWith(".gif"))
  {
    return "image/gif";
  }
  else if (filename.endsWith(".jpg"))
  {
    return "image/jpeg";
  }
  else if (filename.endsWith(".ico"))
  {
    return "image/x-icon";
  }
  else if (filename.endsWith(".xml"))
  {
    return "text/xml";
  }
  else if (filename.endsWith(".pdf"))
  {
    return "application/x-pdf";
  }
  else if (filename.endsWith(".zip"))
  {
    return "application/x-zip";
  }
  else if (filename.endsWith(".gz"))
  {
    return "application/x-gzip";
  }

  return "text/plain";
}

void sendChunkedGetData(std::map<int, TianBMSData> buff)
{
  // server.sendHeader("Cache-Control", "no-cache");
  // server.sendHeader("Pragma", "no-cache");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  server.sendContent("{\"data\":[");
  std::map<int, TianBMSData>::iterator it;
  TianBMSJsonManager jman;
  it = buff.begin();
  while (it != buff.end())
  {
    String content = jman.buildData((*it).second);
    it++;
    if (it != buff.end())
    {
      server.sendContent(content+",");
    }
    else
    {
      server.sendContent(content);
    }
  }
  server.sendContent("]}");
  server.sendContent("");
}

/**
 * Read the file and write the file into webpage using server.sendContent
 * 
 * @param path  filepath to be read and write into server
 * 
 * @return  true when success, false when failed
*/
bool handleFileRead(String path)
{
  ESP_LOGI(TAG, "handleFileRead: %s\n", path.c_str());
  String contentType = getContentType(path);
  String pathWithGz = path;
  ESP_LOGI(TAG, "path with gz : %s\n", pathWithGz.c_str());
  ESP_LOGI(TAG, "path : %s\n", path.c_str());
  ESP_LOGI(TAG, "content type : %s\n", contentType.c_str());
  if (LittleFS.exists(path))
  {
    ESP_LOGI(TAG, "file exists");
    File file = LittleFS.open(path);
    // server.sendHeader("Cache-Control", "no-cache");
    // server.sendHeader("Pragma", "no-cache");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, contentType, "");
    size_t fileSize = 0;
    ESP_LOGI(TAG, "Available filesize : %d\n", file.available());
    while (file.available())
    {
      std::array<char, 512> buff;
      size_t length;
      // Serial.printf("File available to read %d\n", file.available());
      if (file.available() >= buff.max_size())
      {
        length = buff.max_size();
      }
      else
      {
        length = file.available();
      }
      fileSize += file.readBytes(buff.data(), length);
      server.sendContent(buff.data(), length);
    }
    ESP_LOGI(TAG, "Copied filesize : %d\n", fileSize);
    file.close();
    return true;
  }
  return false;
}

/**
 * Handle firmware upload
 * 
 * @brief it is used as an OTA firmware upload, pass the http received file into Update class which handle for flash writing
*/
void handleFirmwareUpload()
{
  // ESP_LOGI(TAG, "Handle firmware upload");
  ethernetHTTPUpload& upload = server.upload();
  FlashZ &flashZ = FlashZ::getInstance();
  static size_t bytesWritten = 0;
  if (upload.status == UPLOAD_FILE_START)
  {
    String filename = upload.filename;
    if (!filename.startsWith("/"))
    {
      filename = "/" + filename;
    }
    ESP_LOGI(TAG, "file name : %s\n", filename.c_str());
    flashZ.begin(UPDATE_SIZE_UNKNOWN);
    ESP_LOGI(TAG, "content length : %d\n", upload.contentLength);
  }
  else if (upload.status == UPLOAD_FILE_WRITE) // buffer data is file content
  {   
    bytesWritten += flashZ.write(upload.buf, upload.currentSize);
    ESP_LOGI(TAG, "uploaded file size : %d\n", upload.totalSize);
  }
  else if (upload.status == UPLOAD_FILE_END) // last buffer data is for non file content such as header
  {
    ESP_LOGI(TAG, "uploaded file size : %d\n", upload.totalSize);
    ESP_LOGI(TAG, "flash written bytes / file size : %d / %d", bytesWritten, upload.totalSize);
    bytesWritten = 0;
  }
}

// /**
//  * Handle firmware upload
//  * 
//  * @brief it is used as an OTA firmware upload, pass the http received file into Update class which handle for flash writing
// */
// void handleFirmwareUpload()
// {
//   // ESP_LOGI(TAG, "Handle firmware upload");
//   ethernetHTTPUpload& upload = server.upload();
//   static size_t bytesWritten = 0;
//   static bool isValid = false;
//   if (upload.status == UPLOAD_FILE_START)
//   {
//     String filename = upload.filename;
//     if (!filename.startsWith("/"))
//     {
//       filename = "/" + filename;
//     }
//     ESP_LOGI(TAG, "file name : %s\n", filename.c_str());
//     isValid = Update.begin(upload.contentLength, U_FLASH);
//     if (!isValid) return;
//     ESP_LOGI(TAG, "content length : %d\n", upload.contentLength);
//   }
//   else if (upload.status == UPLOAD_FILE_WRITE) // buffer data is file content
//   {
//     if(!isValid)
//     {
//       return;
//     }   
//     bytesWritten += Update.write(upload.buf, upload.currentSize);
//     ESP_LOGI(TAG, "uploaded file size : %d\n", upload.totalSize);
//   }
//   else if (upload.status == UPLOAD_FILE_END) // last buffer data is for non file content such as header
//   {
//     if (!isValid) return;
//     ESP_LOGI(TAG, "uploaded file size : %d\n", upload.totalSize);
//     ESP_LOGI(TAG, "flash written bytes / file size : %d / %d", bytesWritten, upload.totalSize);
//     // bytesWritten += Update.write(upload.buf, 0);
//     if (!Update.end(true))
//     {
//       ESP_LOGI(TAG, "Error writing to flash");
//       Update.printError(Serial);
//     }
//     isValid = false;
//     bytesWritten = 0;
//   }
// }

/**
 * Handle compressed firmware upload
 * 
 * @brief it is used as an OTA firmware upload, pass the http received file into Flashz class which handle for flash writing
*/
void handleCompressedFirmwareUpload()
{
  ethernetHTTPUpload& upload = server.upload();
  FlashZ &fz = FlashZ::getInstance();
  static int bytesWritten = 0;
  if (upload.status == UPLOAD_FILE_START)
  {
    String filename = upload.filename;
    if (!filename.startsWith("/"))
    {
      filename = "/" + filename;
    }
    ESP_LOGI(TAG, "file name : %s\n", filename.c_str());
    fz.beginz(UPDATE_SIZE_UNKNOWN);
    ESP_LOGI(TAG, "content length : %d\n", upload.contentLength);
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    bytesWritten = fz.writez(upload.buf, upload.currentSize, false);
    ESP_LOGI(TAG, "uploaded file size : %d\n", upload.totalSize);
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    uint8_t dummyData = 0;
    bytesWritten += fz.writez(&dummyData, 0, true); // to signal end of data, must be used!!
    ESP_LOGI(TAG, "uploaded file size : %d\n", upload.totalSize);
    ESP_LOGI(TAG, "flash written bytes / file size : %d / %d", bytesWritten, upload.totalSize);
    bytesWritten = 0;
  }
}

/**
 * Handle filesystem upload
 * 
 * @brief it is used as an OTA filesystem upload, pass the http received file into Update class which handle for flash writing
*/
void handleFilesystemUpload()
{
  ethernetHTTPUpload& upload = server.upload();
  FlashZ &flashZ = FlashZ::getInstance();
  static size_t bytesWritten = 0;
  if (upload.status == UPLOAD_FILE_START)
  {
    String filename = upload.filename;
    if (!filename.startsWith("/"))
    {
      filename = "/" + filename;
    }
    ESP_LOGI(TAG, "file name : %s\n", filename.c_str());
    flashZ.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS);
    ESP_LOGI(TAG, "content length : %d\n", upload.contentLength);
  }
  else if (upload.status == UPLOAD_FILE_WRITE) // buffer data is file content
  {   
    bytesWritten += flashZ.write(upload.buf, upload.currentSize);
    ESP_LOGI(TAG, "uploaded file size : %d\n", upload.totalSize);
  }
  else if (upload.status == UPLOAD_FILE_END) // last buffer data is for non file content such as header
  {
    ESP_LOGI(TAG, "uploaded file size : %d\n", upload.totalSize);
    ESP_LOGI(TAG, "flash written bytes / file size : %d / %d", bytesWritten, upload.totalSize);
    bytesWritten = 0;
  }
}

/**
 * Handle compressed filesystem upload
 * 
 * @brief it is used as an OTA filesystem upload, pass the http received file into Flashz class which handle for flash writing
*/
void handleCompressedFilesystemUpload()
{
  ethernetHTTPUpload& upload = server.upload();
  FlashZ &fz = FlashZ::getInstance();
  static int bytesWritten = 0;
  if (upload.status == UPLOAD_FILE_START)
  {
    String filename = upload.filename;
    if (!filename.startsWith("/"))
    {
      filename = "/" + filename;
    }
    ESP_LOGI(TAG, "file name : %s\n", filename.c_str());
    fz.beginz(UPDATE_SIZE_UNKNOWN, U_SPIFFS);
    ESP_LOGI(TAG, "content length : %d\n", upload.contentLength);
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    bytesWritten = fz.writez(upload.buf, upload.currentSize, false);
    ESP_LOGI(TAG, "uploaded file size : %d\n", upload.totalSize);
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    uint8_t dummyData = 0;
    bytesWritten += fz.writez(&dummyData, 0, true); // to signal end of data, must be used!!
    ESP_LOGI(TAG, "uploaded file size : %d\n", upload.totalSize);
    ESP_LOGI(TAG, "flash written bytes / file size : %d / %d", bytesWritten, upload.totalSize);
    bytesWritten = 0;
  }
}

void handleNotFound() {
  int internalLed = 2;
  digitalWrite(internalLed, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  server.method();
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
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

/**
 * setup ethernet
 * 
 * @brief adjust the ethernet to either STATIC or DHCP based on saved setting in preferences
*/
void setupEthernet()
{
  SPI.begin(21, 18, 19, 22);
  Ethernet.init(22);
  // std::array<uint8_t, 6> macAddr;
  std::array<uint8_t, 6> macAddr = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x01 };
  // ethernetSave.getMac(macAddr.data(), macAddr.size());
  switch (ethernetSave.getServer())
  {
  case server_type::STATIC:
    ESP_LOGI(TAG, "static");
    Ethernet.begin(macAddr.data(), IPAddress(192, 168, 2, 26));
    break;
  case server_type::DHCP:
    ESP_LOGI(TAG, "dhcp");
    while (!Ethernet.begin(macAddr.data(), 5000, 1000))
    {
      ESP_LOGI(TAG, "failed to connect");
    }
    break;
  default:
    break;
  }
  
  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    // Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    ESP_LOGI(TAG, "Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    while (true) {
      delay(1); // do nothing, no point running without Ethernet hardware
    }
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    ESP_LOGI(TAG, "Ethernet cable is not connected.");
  }
  ESP_LOGI(TAG, "Connected to IP : %s\n", Ethernet.localIP().toString());
  ESP_LOGI(TAG, "MAC Address %02X::%02X::%02X::%02X::%02X::%02X", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
}

/**
 * Http task
 * 
 * @brief this is http task in freertos, its task is to handle client request to serve webpage and response to api request (POST / GET)
*/
void httpTask(void *pv)
{
  UBaseType_t uxHighWaterMark;
  uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
  while (1)
  {
    // ESP_LOGI(TAG, "http task");
    server.handleClient();
    uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
    // ESP_LOGI(TAG, "unused stack in words : %d\n", uxHighWaterMark);
    vTaskDelay(1);  // add delay for other task to execute
  }
}

void flashOnProgress(size_t currentSize, size_t totalSize)
{
  ESP_LOGI(TAG, "current size : %d\n", currentSize);
  ESP_LOGI(TAG, "total size : %d\n", totalSize);
}

void setup() {
  // put your setup code here, to run once:
    
  xTaskCreatePinnedToCore(
    httpTask, 
    "http task", 
    8192, 
    NULL, 
    1, 
    &httpTaskHandle, 
    1
  );

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

  ethernetSave.begin("eth_param");
  // ethernetSave.clear();
  // while(1);

  talis5Memory.begin("talis5_param");
  // talis5Memory.clear();
  // while(1);
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

  setupEthernet();

  RTUutils::prepareHardwareSerial(Serial2);
  Serial2.begin(9600);

  MB.onDataHandler(&handleData);
  MB.onErrorHandler(&handleError);
  MB.setTimeout(1000);
  // MB.begin(Serial2);

  FlashZ &fz = FlashZ::getInstance();
  fz.onProgress(flashOnProgress);

  server.on("/", HTTP_GET, [](){
      if (!handleFileRead("/index.html"))
      {
        server.send(404, "text/plain", "FileNotFound");
      }
      else
      {
        Serial.println("Success serve webpage");
        server.sendContent("");
      }
  });

  server.on("/update", HTTP_GET, [](){
      if (!handleFileRead("/update.html"))
      {
        server.send(404, "text/plain", "FileNotFound");
      }
      else
      {
        Serial.println("Success serve webpage");
        server.sendContent("");
      }
  });

  server.on("/assets/progressUpload.js", HTTP_GET, [](){
      if (!handleFileRead("/assets/progressUpload.js"))
      {
        server.send(404, "text/plain", "FileNotFound");
      }
      else
      {
        server.sendContent("");
      }
  });

  server.on("/assets/function_info_page.js", HTTP_GET, [](){
      if (!handleFileRead("/assets/function_info_page.js"))
      {
        server.send(404, "text/plain", "FileNotFound");
      }
      else
      {
        server.sendContent("");
      }
  });

  server.on("/assets/index.css", HTTP_GET, [](){
      if (!handleFileRead("/assets/index.css"))
      {
        server.send(404, "text/plain", "FileNotFound");
      }
      else
      {
        server.sendContent("");
      }
  });

  server.on("/assets/info.css", HTTP_GET, [](){
      if (!handleFileRead("/assets/info.css"))
      {
        server.send(404, "text/plain", "FileNotFound");
      }
      else
      {
        server.sendContent("");
      }
  });

  server.on("/assets/update_styles.css", HTTP_GET, [](){
      if (!handleFileRead("/assets/update_styles.css"))
      {
        server.send(404, "text/plain", "FileNotFound");
      }
      else
      {
        server.sendContent("");
      }
  });

  server.on("/favicon.ico", HTTP_GET, [](){
      if (!handleFileRead("/assets/favicon.ico"))
      {
        server.send(404, "text/plain", "FileNotFound");
      }
      else
      {
        server.sendContent("");
      }
  });

  server.on("/api/get-data", HTTP_GET, [](){
      std::map<int, TianBMSData> buffer = reader.getTianBMSData();
      ESP_LOGI(TAG, "api get data");
      sendChunkedGetData(buffer);
  });

  server.on("/api/get-device-info", HTTP_GET, [](){
      JsonDocument doc;
      String output;
      doc["firmware_version"] = FIRMWARE_VERSION;
      switch (ethernetSave.getServer())
      {
      case 1:
          /* code */
          doc["server_mode"] = "STATIC";
          break;
      case 2:
          doc["server_mode"] = "DHCP";
          break;
      default:
          break;
      }
      doc["device_ip"] = Ethernet.localIP().toString();
      doc["gateway"] = Ethernet.gatewayIP().toString();
      doc["subnet"] = Ethernet.subnetMask().toString();
      std::array<uint8_t, 6> mac;
      Ethernet.MACAddress(mac.data());
      std::array<char, 32> buffer;
      snprintf(buffer.data(), buffer.size(), "%02X::%02X::%02X::%02X::%02X::%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      String value = String(buffer.data());
      doc["mac_address"] = value;
      serializeJson(doc, output);
      server.send(200, "application/json", output);
  });

  server.on("/api/get-modbus-info", HTTP_GET, []()
  {
      JsonDocument doc;
      String output;
      doc["modbus_ip"] = talis5Memory.getModbusTargetIp();
      doc["port"] = talis5Memory.getModbusPort();
      std::vector<uint8_t> buff;
      buff.reserve(255);
      buff.resize(talis5Memory.getSlaveSize());
      size_t slaveNum = talis5Memory.getSlave(buff.data(), talis5Memory.getSlaveSize());
      JsonArray slave_list = doc["slave_list"].to<JsonArray>();
      for (size_t i = 0; i < buff.size(); i++)
      {
          slave_list.add(buff.at(i));
      }
      doc["baudrate"] = talis5Memory.getBaudRate();

      serializeJson(doc, output);
      server.send(200, "application/json", output); });

  server.on("/api/get-active-slave", HTTP_GET, []()
  {
      JsonDocument doc;
      String output;
      doc["address_status"] = isScanFinished;
      JsonArray slave_list = doc["slave_list"].to<JsonArray>();
      std::map<int, TianBMSData>::iterator it;
      for (it = reader.getTianBMSData().begin(); it != reader.getTianBMSData().end(); it++)
      {
          slave_list.add((*it).first);
      }

      serializeJson(doc, output);

      server.send(200, "application/json", output); });

  server.on("/api/update-firmware", HTTP_POST, [](){
    FlashZ &flashZ = FlashZ::getInstance();
    String errMsg;
    if (!flashZ.end(true))
    {
      ESP_LOGI(TAG, "Error writing to flash");
      errMsg = String(flashZ.errorString());
      ESP_LOGI(TAG, "error message : %s\n", flashZ.errorString());
      server.send(200, "text/plain", errMsg);
    }
    else
    {
      server.send(200, "text/plain", "File has been uploaded successfully.");
      isRestart = true;
    }
  }, handleFirmwareUpload);

  server.on("/api/update-compressed-firmware", HTTP_POST, [](){
    FlashZ &flashZ = FlashZ::getInstance();
    String errMsg;
    if (!flashZ.endz(true))
    {
      ESP_LOGI(TAG, "Error writing to flash");
      errMsg = String(flashZ.errorString());
      ESP_LOGI(TAG, "error message : %s\n", flashZ.errorString());
      server.send(200, "text/plain", errMsg);
    }
    else
    {
      server.send(200, "text/plain", "File has been uploaded successfully.");
      isRestart = true;
    }
  }, handleCompressedFirmwareUpload);

  server.on("/api/update-filesystem", HTTP_POST, [](){
    FlashZ &flashZ = FlashZ::getInstance();
    String errMsg;
    if (!flashZ.end(true))
    {
      ESP_LOGI(TAG, "Error writing to flash");
      errMsg = String(flashZ.errorString());
      ESP_LOGI(TAG, "error message : %s\n", flashZ.errorString());
      server.send(200, "text/plain", errMsg);
    }
    else
    {
      server.send(200, "text/plain", "File has been uploaded successfully.");
      isRestart = true;
    }
  }, handleFilesystemUpload);

  server.on("/api/update-compressed-filesystem", HTTP_POST, [](){
    FlashZ &flashZ = FlashZ::getInstance();
    String errMsg;
    if (!flashZ.endz(true))
    {
      ESP_LOGI(TAG, "Error writing to flash");
      errMsg = String(flashZ.errorString());
      ESP_LOGI(TAG, "error message : %s\n", flashZ.errorString());
      server.send(200, "text/plain", errMsg);
    }
    else
    {
      server.send(200, "text/plain", "File has been uploaded successfully.");
      isRestart = true;
    }
  }, handleCompressedFilesystemUpload);

  server.on("/api/set-slave", HTTP_POST, [](){
      ESP_LOGI(TAG, "----------------set slave----------------");
      ESP_LOGI(TAG, "%s\n", server.arg("plain").c_str());
      Talis5JsonHandler handler;
      int status = 400;
      std::vector<uint8_t> buff;
      size_t len = handler.parseSetSlaveJson(server.arg("plain"), buff);
      if (len != 0)
      {
          status = 200;
          talis5Memory.setSlave(buff.data(), len);
          talis5Memory.printUser();
          talis5Memory.cancel();
          // talis5Memory.save();
          isSlaveChanged = true;
          lastQueueCheck = millis();
      }
      String response = handler.buildJsonResponse(status);
      server.send(status, "application/json", response);
      return;
  });

  server.on("/api/scan", HTTP_POST, [](){
      Talis5JsonHandler handler;
      int status = 400;
      if (handler.parseModbusScan(server.arg("plain")))
      {
          status = 200;
          isScan = true;
      }
      server.send(status, "application/json", handler.buildJsonResponse(status));
  });

  server.on("/api/set-modbus", HTTP_POST, [](){
      ESP_LOGI(TAG, "----------------set modbus----------------");
      Talis5JsonHandler handler;
      Talis5ParameterData param;
      int status = 400;
      if (handler.parseSetModbus(server.arg("plain"), param))
      {
          status = 200;
          talis5Memory.setModbusTargetIp(param.modbusTargetIp);
          talis5Memory.setModbusPort(param.modbusPort);
          talis5Memory.cancel();
          // talis5Memory.save();
          IPAddress ip;
          if (ip.fromString(param.modbusTargetIp))
          {
            // MB.setTarget(ip, param.modbusPort);
          }
      }
      server.send(status, "application/json", handler.buildJsonResponse(status));
  });

  server.on("/api/set-serial-modbus", HTTP_POST, [](){
      ESP_LOGI(TAG, "----------------set modbus----------------");
      Talis5JsonHandler handler;
      Talis5ParameterData param;
      int status = 400;
      if (handler.parseSetSerialModbus(server.arg("plain"), param))
      {
          status = 200;
          talis5Memory.setBaudRate(param.baudRate);
          talis5Memory.printUser();
          talis5Memory.cancel();
          // talis5Memory.save();
      }
      server.send(status, "application/json", handler.buildJsonResponse(status));
  });

  server.on("/api/restart", HTTP_POST, [](){
      Talis5JsonHandler handler;
      int status = 400;
      if (handler.parseRestart(server.arg("plain")))
      {
          status = 200;
          isRestart = true;
      }
      server.send(status, "application/json", handler.buildJsonResponse(status));
  });

  server.on("/api/set-network", HTTP_POST, [](){
      ESP_LOGI(TAG, "----------------set network----------------");
      int status = 400;
      EthernetParameterData ethernetParameterData;
      Talis5JsonHandler parser;
      if (parser.parseSetNetwork(server.arg("plain"), ethernetParameterData))
      {
          status = 200;
          ESP_LOGI(TAG, "server : %d\n", ethernetParameterData.server);
          ESP_LOGI(TAG, "ip : %s\n", ethernetParameterData.ip.c_str());
          ESP_LOGI(TAG, "gateway : %s\n", ethernetParameterData.gateway.c_str());
          ESP_LOGI(TAG, "subnet : %s\n", ethernetParameterData.subnet.c_str());
          ethernetSave.setServer(ethernetParameterData.server);
          ethernetSave.setIp(ethernetParameterData.ip);
          ethernetSave.setGateway(ethernetParameterData.gateway);
          ethernetSave.setSubnet(ethernetParameterData.subnet);
          ethernetSave.setMac(ethernetParameterData.mac.data(), ethernetParameterData.mac.size());
          ethernetSave.printUser();
          ethernetSave.cancel();
          // ethernetSave.save();
      }
      server.send(status, "application/json", parser.buildJsonResponse(status));
  });

  server.on("/api/freset", HTTP_POST, [](){
      Talis5JsonHandler handler;
      int status = 400;
      if (handler.parseFactoryReset(server.arg("plain")))
      {
          status = 200;
          talis5Memory.reset();
          ethernetSave.reset();
          isRestart = true;
      }
      server.send(status, "application/json", handler.buildJsonResponse(status));
  });

  // server.onNotFound(handleNotFound);
  server.begin();
  globalIterator = reader.getTianBMSData().begin();
  lastRequest = millis();
  lastCleanup = millis();
}

unsigned long lastCheck = 0;

void loop() {
  // put your main code here, to run repeatedly:
    
    if (millis() - lastCheck > 1000)
    {
      ESP_LOGI(TAG, "ULULULU");
      lastCheck = millis();
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

    // if(isSlaveChanged || isScan)
    // {
    //     if (MB.pendingRequests() == 0)
    //     {
    //         if (millis() - lastQueueCheck > 3000)
    //         {
    //             if (xSemaphoreTake(read_mutex, portMAX_DELAY))
    //             {
    //                 if (xSemaphoreTake(write_mutex, portMAX_DELAY))
    //                 {
    //                     // check if the signal is coming from slave changed flag, the new setting need to be written, if not it is coming from rescan flag
    //                     if (isSlaveChanged) 
    //                     {
    //                         ESP_LOGI(TAG, "SAVE PARAMETER AND CLEAR");
    //                         talis5Memory.save();
    //                         slave.resize(talis5Memory.getSlaveSize());
    //                         talis5Memory.getSlave(slave.data(), talis5Memory.getSlaveSize());
    //                     }
    //                     else
    //                     {
    //                         ESP_LOGI(TAG, "RESCAN");
    //                     }
    //                     isScanFinished = false;
    //                     isSlaveChanged = false;
    //                     isScan = false;
    //                     reader.clearData();
    //                     xSemaphoreGive(write_mutex);
    //                 }
    //                 xSemaphoreGive(read_mutex);
    //             }
    //             lastQueueCheck = millis();
    //         }
    //     }
    //     else
    //     {
    //         lastQueueCheck = millis();
    //     }
    // }

  //   if (isScanFinished)
  //   {
  //       /**
  //        * This block will push the request to queue based on detected slave
  //       */
  //       // if (!isCleanup) // this flag is to detect if it's time to do cleanup, then pause the .addRequest
  //       // { 
  //       if (!isSlaveChanged && !isScan) // if it is not scan or not slave changed, do normal polling
  //       {
  //           if (globalIterator != reader.getTianBMSData().end())
  //           {
  //               if (millis() - lastRequest > 500)
  //               {
  //                   // 43 request is for all data
  //                   // Error err = MB.addRequest(reader.getToken((*globalIterator).first, TianBMSUtils::REQUEST_DATA), (*globalIterator).first, READ_HOLD_REGISTER, 4096, 43);
  //                   // 34 request for old data
  //                   Error err = MB.addRequest(reader.getToken((*globalIterator).first, TianBMSUtils::REQUEST_DATA), (*globalIterator).first, READ_INPUT_REGISTER, 4096, 34);
  //                   if (err!=SUCCESS) {
  //                       ModbusError e(err);
  //                       Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
  //                   }
  //                   lastRequest = millis();
  //                   globalIterator++; // increment the iterator to the next element
  //               }
  //           }
  //           else
  //           {
  //               globalIterator = reader.getTianBMSData().begin(); // get the iterator of the first element of data
  //           }
  //       }
  //       // }
  //   }
  //   else
  //   {
  //       /**
  //        * This block is to do address(slave) scan, start from slave 1 to 16
  //       */
  //       if (millis() - lastRequest > 500)
  //       {
  //           ESP_LOGI(TAG, "Slave pointer : %d\n", slavePointer);
  //           ESP_LOGI(TAG, "Slave address : %d\n", slave.at(slavePointer));
  //           // Error err = MB.addRequest(reader.getToken(slave.at(slavePointer), TianBMSUtils::REQUEST_SCAN), slave.at(slavePointer), READ_HOLD_REGISTER, 4096, 1);
  //           Error err = MB.addRequest(reader.getToken(slave.at(slavePointer), TianBMSUtils::REQUEST_SCAN), slave.at(slavePointer), READ_INPUT_REGISTER, 4096, 1);
  //           if (err!=SUCCESS) {
  //               ModbusError e(err);
  //               Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
  //           }
  //           slavePointer++;
  //           if (slavePointer >= slave.size())
  //           {
  //               slavePointer = 0;
  //               isScanFinished = 1;
  //               globalIterator = reader.getTianBMSData().begin();
  //           }
  //           lastRequest = millis();
  //       }
  //   }
    
    
    

  //   // ESP_LOGI(TAG, "PCB Code : %s\n", tianBMS.getPcbBarcode().c_str());
	// // if (factoryReset)
	// // {
	// // 	talisMemory.reset();
	// // 	ESP_LOGI(TAG, "Factory Reset");
	// // 	rmsRestartCommand.restart = true;
	// // 	factoryReset = false;
	// // }

	if (isRestart)
	{
		isRestart = false;
		digitalWrite(internalLed, LOW);
		delay(100);
		ESP.restart();
	}
  // ArduinoOTA.handle();
  Ethernet.maintain();
  delay(1);
}