/*
 * Common.h
 * this file will hold common methods used ion main app
 * 
 */

#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <cstdlib>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
//https://pubsubclient.knolleary.net/api docs for api

#include <WiFiClientSecure.h>
#include <base64.h>
#include <bearssl/bearssl.h>
#include <bearssl/bearssl_hmac.h>
#include <libb64/cdecode.h>

#include <az_result.h>
#include <az_span.h>
#include <az_iot_hub_client.h>

// arduino json library
#include <ArduinoJson.h>

// cert required for mqtt
#include "ca.h"

// for reading sd card
#include <SPI.h>
#include <SD.h>
//#include "../../../Arduino/libraries/SD/src/SD.h"


// setup https client for 8266 SSL client
static WiFiClientSecure wifi_client;
static PubSubClient mqtt_client(wifi_client);
static az_iot_hub_client client;

#define sizeofarray(a) (sizeof(a) / sizeof(a[0]))
#define ONE_HOUR_IN_SECS 3600
#define NTP_SERVERS "pool.ntp.org", "time.nist.gov"
#define MQTT_PACKET_SIZE 1024

static char sas_token[200];
static uint8_t signature[512];
static unsigned char encrypted_signature[32];
static char base64_decoded_device_key[32];
static unsigned long next_telemetry_send_time_ms = 0;
static char telemetry_topic[128];
static uint8_t telemetry_payload[100];
static uint32_t telemetry_send_count = 0;
static const int port = 8883;

String passData[7];
int  timedelay, Interval;

// declare functions
void httpRequest(String verb, String uri, String host, String sas, String contentType, String content);
String createJsonData(String devId, float temp ,float humidity,float pressure, String keyid);
void sendToDisplay(int col, int row, String data);
void sendToDisplay(int col, int row, String data,int Style);
void clearDisplay();


String createJsonData(String devId,  float temp ,float humidity,float pressure,float alt, int Interval, int timedelay, String keyid)
{ 
  // create json object
  StaticJsonDocument<256> root;
  char* buff;
  String outdata;

  // remove trailing \n and spaces
  keyid = keyid.substring( 0,keyid.indexOf('\n') );
  keyid.replace(' ','_');
  
  root["DeviceId"] = devId;
  root["Time"] = keyid;
  root["temperature"] = temp;
  root["humidity"] = humidity;
  root["pressure"] = pressure;
  root["altitude"] = alt;
  root["Interval"] = Interval;
  root["timedelay"] = timedelay;
  
  // convert to string
  serializeJson(root, outdata);
 
  sendToDisplay(0, 48, outdata,STYLE_BOLD);
  return outdata;

}

void clearDisplay()
{
  ssd1306_clearScreen();
}

void sendToDisplay(int col, int row, String data)
{
   ssd1306_negativeMode();
   ssd1306_printFixed(col,  row, (const char*)data.c_str(), STYLE_BOLD);
   ssd1306_positiveMode();
}

void sendToDisplay(int col, int row, String data, int Style)
{
  /*
  * printing options
  * https://lexus2k.github.io/ssd1306/group___l_c_d__1_b_i_t___g_r_a_p_h_i_c_s.html#ga30a785aa6d528a3fddbe2de853211fb9
  * 
  */
  switch (Style)
  {
    case STYLE_NORMAL:
     ssd1306_printFixed(col,  row, (const char*)data.c_str(), STYLE_NORMAL);
     break;

    case STYLE_BOLD:
     ssd1306_negativeMode();
     ssd1306_printFixed(col,  row, (const char*)data.c_str(), STYLE_BOLD);
     ssd1306_positiveMode();
     break;
    
    case STYLE_ITALIC:
     ssd1306_printFixed(col,  row, (const char*)data.c_str(), STYLE_ITALIC);
     break;

    default:
     ssd1306_printFixed(col,  row, (const char*)data.c_str(), STYLE_NORMAL);
     break;
     
  }
 
}

void connectToWiFi(String *passData)
{
  
String netid, pwd;

  // move sd card data to global variables
  netid = passData[0];
  pwd = passData[1];
 
  // initialize wifi
  WiFi.disconnect();
  WiFi.begin( (const char*)netid.c_str() , (const char*)pwd.c_str() );
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  
    switch (WiFi.status())  
    {
    case WL_CONNECTION_LOST:
      Serial.println("Connection Lost");
      break;
    case WL_CONNECT_FAILED:
      Serial.println("Connection Failed");
      break;
    case WL_DISCONNECTED:
      Serial.println(" Not Connected");
      break;
    default:
      Serial.print("Status:");
      Serial.println(WiFi.status());
      break;
    }
    clearDisplay();
    sendToDisplay(0,8,"Connecting",STYLE_NORMAL);
    sendToDisplay(0,16,"..",STYLE_NORMAL);
    sendToDisplay(8,24,"...",STYLE_NORMAL);
  }

  // connected show ip address
  clearDisplay();
  sendToDisplay(0,8,"Connected:" + netid,STYLE_NORMAL);
  sendToDisplay(0,16, "IP:" + WiFi.localIP().toString(),STYLE_NORMAL);
  delay(2000);
  Serial.println("done!");
}

void getSDData(String *passData)
{

  String str;
  File dataFile;
  
  Serial.println("In getSDData");
  if (!SD.begin(SS)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");
  
  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  dataFile = SD.open("wifiFile.txt");
   
  int index = 0;

  if (dataFile)
  {
    Serial.println("data from sd card");
    
    while (dataFile.available())
    {
      if (dataFile.find("SSID:"))
      {
        str = dataFile.readStringUntil('|');
        passData[0]  = str;
        Serial.println(str);
        sendToDisplay(0,8,str,STYLE_NORMAL);
      }
      if (dataFile.find("PASSWORD:"))
      {
        str = dataFile.readStringUntil('|');
        passData[1]  = str;
        Serial.println(str);
        sendToDisplay(0,16, str,STYLE_NORMAL);
      }
      if (dataFile.find("DEVICEID:"))
      {
        str = dataFile.readStringUntil('|');
        passData[2]  = str;
        Serial.println(str);
        sendToDisplay(0,24, str,STYLE_BOLD);
      }
      
      if (dataFile.find("HOSTNAME:"))
      {
        str = dataFile.readStringUntil('|');
        passData[3]  = str;
        Serial.println(str);
        sendToDisplay(0, 40, str,STYLE_NORMAL);
      }
      if (dataFile.find("SAS:"))
      {
        str = dataFile.readStringUntil('|');
        passData[4]  = str;
        Serial.println(str);
      }
      if (dataFile.find("DELAY:"))
      {
        str = dataFile.readStringUntil('|');
        passData[5]  = str;
        Serial.println(str);
        sendToDisplay(0, 48, "delay:" + str,STYLE_NORMAL);
      }
      // interval will be how many readings to wait till we send data to azure.
      // needed to limit records sent as free hub has limit of 8000 messages / day
      if (dataFile.find("INTERVAL:"))
      {
        str = dataFile.readStringUntil('|');
        passData[6]  = str;
        Serial.println(str);
        sendToDisplay(0, 56, "INTERVAL:" + str,STYLE_NORMAL);
      }
      
    }
    // close the file
    dataFile.close();
  }

      
}

static void initializeTime()
{
  Serial.print("Setting time using SNTP");
  sendToDisplay(0,32, "Setting time using SNTP");
  
  configTime(-5 * 3600, 0, NTP_SERVERS);
  time_t now = time(NULL);
  while (now < 1510592825)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("done!");
  sendToDisplay(0,40, "Done!");
  delay(1000);
}

static char* getCurrentLocalTimeString()
{
  time_t now = time(NULL);
  return ctime(&now);
}

static void printCurrentTime()
{
  Serial.print("Current time: ");
  Serial.print(getCurrentLocalTimeString());

  clearDisplay();
  sendToDisplay(0,16, getCurrentLocalTimeString() );
  delay(1000);
}

void setTimeMethod(String cmdName, JsonVariant jsonValue)
{
  String payloadMsg = "";
  Serial.println("method");
  Serial.print(cmdName);
  Serial.println("");
  delay(3000);
  
}

void receivedCallback(char* topic, byte* payload, unsigned int length)
{  
  StaticJsonDocument<256> root;
  String payloadMsg;
  
  Serial.println("");
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println("");


  /*
   * Message format is a json string s follows
   * {"TimeDelay" : int,
      "Interval" : int
     }
  */
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
    payloadMsg += (char)payload[i];
  }
  
  Serial.println("");
   
  clearDisplay();
  sendToDisplay(0, 8, "Recieved message from Cloud" ,STYLE_NORMAL);
  sendToDisplay(0, 16, payloadMsg ,STYLE_NORMAL);

  // deserialize json message
  deserializeJson( root,payloadMsg);
  String tm = root["TimeDelay"];
  String intv = root["Interval"];
  Serial.print("TimeDelay : ");
  Serial.println(tm);
  
  Serial.print("Interval : ");
  Serial.println(intv);

  // set new delay between readings and send interval
  timedelay = tm.toInt();
  passData[5] =  tm.toInt();
  
  Interval = intv.toInt();  
  passData[6] =  intv.toInt();

  sendToDisplay(0, 40, "New Delay:" ,STYLE_NORMAL);
  sendToDisplay(75, 40, tm ,STYLE_NORMAL);

  sendToDisplay(0, 48, "New Interval:" ,STYLE_NORMAL);
  sendToDisplay(75, 48, intv ,STYLE_NORMAL);
  
  delay(3000);
    
}

static void initializeClients(const char* device_id,const char* host)
{

  if (!wifi_client.setCACert((const uint8_t*)ca_pem, ca_pem_len))
  {
    Serial.println("setCACert() FAILED");
    return;
  }

  if (az_result_failed(az_iot_hub_client_init(
          &client,
          az_span_create((uint8_t*)host, strlen(host)),
          az_span_create((uint8_t*)device_id, strlen(device_id)),
          NULL)))
  {
    Serial.println("Failed initializing Azure IoT Hub client");
    return;
  }

  mqtt_client.setServer(host, port);
  mqtt_client.setCallback(receivedCallback);
 

  clearDisplay();
  sendToDisplay(0,16, "Client initialized" );
  delay(1000);
}

static uint32_t getSecondsSinceEpoch()
{
  return (uint32_t)time(NULL);
}

static int generateSasToken(char* sas_token, size_t size,String device_key)
{
  az_span signature_span = az_span_create((uint8_t*)signature, sizeofarray(signature));
  az_span out_signature_span;
  az_span encrypted_signature_span
      = az_span_create((uint8_t*)encrypted_signature, sizeofarray(encrypted_signature));

  uint32_t expiration = getSecondsSinceEpoch() + ONE_HOUR_IN_SECS;

  // Get signature
  if (az_result_failed(az_iot_hub_client_sas_get_signature(
          &client, expiration, signature_span, &out_signature_span)))
  {
    Serial.println("Failed getting SAS signature");
    return 1;
  }

  // Base64-decode device key
  int base64_decoded_device_key_length
      = base64_decode_chars((const char*)device_key.c_str(), strlen((const char*)device_key.c_str()), base64_decoded_device_key);

  if (base64_decoded_device_key_length == 0)
  {
    Serial.println("Failed base64 decoding device key");
    return 1;
  }

  // SHA-256 encrypt
  br_hmac_key_context kc;
  br_hmac_key_init(
      &kc, &br_sha256_vtable, base64_decoded_device_key, base64_decoded_device_key_length);

  br_hmac_context hmac_ctx;
  br_hmac_init(&hmac_ctx, &kc, 32);
  br_hmac_update(&hmac_ctx, az_span_ptr(out_signature_span), az_span_size(out_signature_span));
  br_hmac_out(&hmac_ctx, encrypted_signature);

  // Base64 encode encrypted signature
  String b64enc_hmacsha256_signature = base64::encode(encrypted_signature, br_hmac_size(&hmac_ctx));

  az_span b64enc_hmacsha256_signature_span = az_span_create(
      (uint8_t*)b64enc_hmacsha256_signature.c_str(), b64enc_hmacsha256_signature.length());

  // URl-encode base64 encoded encrypted signature
  if (az_result_failed(az_iot_hub_client_sas_get_password(
          &client,
          expiration,
          b64enc_hmacsha256_signature_span,
          AZ_SPAN_EMPTY,
          sas_token,
          size,
          NULL)))
  {
    Serial.println("Failed getting SAS token");
    return 1;
  }

  return 0;
}

static int connectToAzureIoTHub()
{
  size_t client_id_length;
  char mqtt_client_id[128];
  if (az_result_failed(az_iot_hub_client_get_client_id(
          &client, mqtt_client_id, sizeof(mqtt_client_id) - 1, &client_id_length)))
  {
    Serial.println("Failed getting client id");
    return 1;
  }

  mqtt_client_id[client_id_length] = '\0';

  char mqtt_username[128];
  // Get the MQTT user name used to connect to IoT Hub
  if (az_result_failed(az_iot_hub_client_get_user_name(
          &client, mqtt_username, sizeofarray(mqtt_username), NULL)))
  {
    printf("Failed to get MQTT clientId, return code\n");
    return 1;
  }

  
  clearDisplay();  
  sendToDisplay(0,8, "Client Id" );
  sendToDisplay(0,16, mqtt_client_id );
  Serial.print("Client ID: ");
  Serial.println(mqtt_client_id);

  sendToDisplay(0,24, "UserName" );
  sendToDisplay(0,32, mqtt_username );
  Serial.print("Username: ");
  Serial.println(mqtt_username);

  mqtt_client.setBufferSize(MQTT_PACKET_SIZE);

  while (!mqtt_client.connected())
  {
    time_t now = time(NULL);

    Serial.print("MQTT connecting ... ");

    if (mqtt_client.connect(mqtt_client_id, mqtt_username, sas_token))
    {
      Serial.println("connected.");
      sendToDisplay(0,40, "Connected" );
    }
    else
    {
      sendToDisplay(0,40, "Failed, try again in 5 sec" );
      Serial.print("failed, status code =");
      Serial.print(mqtt_client.state());
      Serial.println(". Try again in 5 seconds.");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }

  mqtt_client.subscribe(AZ_IOT_HUB_CLIENT_C2D_SUBSCRIBE_TOPIC);

  return 0;
}

void establishConnection(String *passData) 
{
  connectToWiFi(passData);
  initializeTime();
  printCurrentTime();
  initializeClients(passData[2].c_str() , passData[3].c_str() );

  // The SAS token is valid for 1 hour by default in this sample.
  // After one hour the sample must be restarted, or the client won't be able
  // to connect/stay connected to the Azure IoT Hub.
  if (generateSasToken(sas_token, sizeofarray(sas_token), passData[4]) != 0)
  {
    Serial.println("Failed generating MQTT password");
  }
  else
  {
    connectToAzureIoTHub();
  }

  
}
