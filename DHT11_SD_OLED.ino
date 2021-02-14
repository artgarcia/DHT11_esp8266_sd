

/*
 DHT11 temperatire and Humidity sensor project

  Part: DHT11 tem sensor
        DiyMall 0.96 oled display
        pololu mini sd card reader
        nodemcu esp8266 chip
        powersupply
        

   ssd1306 driver methods
   https://lexus2k.github.io/ssd1306/group___l_c_d__1_b_i_t___g_r_a_p_h_i_c_s.html#ga30a785aa6d528a3fddbe2de853211fb9

   wire digram for diymall 0.96 oled display
   https://github.com/lexus2k/ssd1306/wiki/Hardware-setup

   DHT11 configuration
   D4 on nodemcu = gpio 2 
   this number is the gpio pin 
   look up gpio topin for node mcu
   https://www.bing.com/images/search?view=detailV2&ccid=HYgKEkqc&id=535B4AD6A8781F348DB52D4C4F0F8288DB8D94E5&thid=OIP.HYgKEkqc4uoc7q1m6KwgkgHaFA&mediaurl=https%3a%2f%2fth.bing.com%2fth%2fid%2fR1d880a124a9ce2ea1ceead66e8ac2092%3frik%3d5ZSN24iCD09MLQ%26riu%3dhttp%253a%252f%252fwww.electronicwings.com%252fpublic%252fimages%252fuser_images%252fimages%252fNodeMCU%252fNodeMCU%2bBasics%2busing%2bArduino%2bIDE%252fNodeMCU%2bGPIO%252fNodeMCU%2bGPIOs.png%26ehk%3dgu%252bgl1WqhjetGSgE%252bDuFj6eD2Kc%252bH4%252fS4XbKgY2WPPI%253d%26risl%3d%26pid%3dImgRaw&exph=1109&expw=1639&q=nodemcu+gpio&simid=608013214873354300&ck=C38CD1D1BB4C9EF6A9A02681F71D4DBB&selectedIndex=0&FORM=IRPRST&ajaxhist=0
                 
*/
#include "ssd1306.h"
#include <Wire.h>
#include <SPI.h>

// common library to store methods used in this sketch
#include "Common.h"

// for dht11 temp sensor on esp8266 chip
#include <DHT.h>

unsigned status;
char netid, pwd, device_id, url, host, sas;
int IntervalCount;
float totTemp, totHumidity, totPressure, totALtitude;

#define DHTPIN 0 // D4 on nodemcu = gpio 2 
                 // this number is the gpio pin 
                 // look up gpio topin for node mcu
                 // https://www.bing.com/images/search?view=detailV2&ccid=HYgKEkqc&id=535B4AD6A8781F348DB52D4C4F0F8288DB8D94E5&thid=OIP.HYgKEkqc4uoc7q1m6KwgkgHaFA&mediaurl=https%3a%2f%2fth.bing.com%2fth%2fid%2fR1d880a124a9ce2ea1ceead66e8ac2092%3frik%3d5ZSN24iCD09MLQ%26riu%3dhttp%253a%252f%252fwww.electronicwings.com%252fpublic%252fimages%252fuser_images%252fimages%252fNodeMCU%252fNodeMCU%2bBasics%2busing%2bArduino%2bIDE%252fNodeMCU%2bGPIO%252fNodeMCU%2bGPIOs.png%26ehk%3dgu%252bgl1WqhjetGSgE%252bDuFj6eD2Kc%252bH4%252fS4XbKgY2WPPI%253d%26risl%3d%26pid%3dImgRaw&exph=1109&expw=1639&q=nodemcu+gpio&simid=608013214873354300&ck=C38CD1D1BB4C9EF6A9A02681F71D4DBB&selectedIndex=0&FORM=IRPRST&ajaxhist=0
                 
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(115200);

  /* 
   *  available fonts
   *  https://lexus2k.github.io/ssd1306/ssd1306__fonts_8h.html 
  */
  ssd1306_setFixedFont(ssd1306xled_font6x8);  
  ssd1306_128x64_i2c_init();
  ssd1306_clearScreen();
  
  /* get data from sd card */
  Serial.print("Initializing SD card...");
  getSDData(passData);

  // get delay between readings and send interval
  timedelay = passData[5].toInt();
  Interval =  passData[6].toInt();

  /* connect to wifi */
  connectToWiFi(passData);
  clearDisplay();
  sendToDisplay(0,16,"now Connected:" + netid);
  sendToDisplay(0,24, "IP:" + WiFi.localIP().toString());
  delay(3000);
  
  initializeTime();
  printCurrentTime();
  initializeClients(passData[2].c_str() , passData[3].c_str() );
  
  // The SAS token is valid for 1 hour by default in this sample.
  // After one hour the sample must be restarted, or the client won't be able
  // to connect/stay connected to the Azure IoT Hub.
  if (generateSasToken(sas_token, sizeofarray(sas_token),passData[4]) != 0)
  {
    Serial.println("Failed generating MQTT password");
  }
  else
  {
    connectToAzureIoTHub();
  }

  clearDisplay();
  // start temp sensor
  sendToDisplay(0, 32, "Sensor Begin");
  dht.begin();
  
}

static void sendTelemetry(String payload)
{
  
  Serial.print(" ESP8266 Sending telemetry . . . ");
  if (az_result_failed(az_iot_hub_client_telemetry_get_publish_topic(
          &client, NULL, telemetry_topic, sizeof(telemetry_topic), NULL)))
  {
    Serial.println("Failed az_iot_hub_client_telemetry_get_publish_topic");
    return;
  }

  mqtt_client.publish(telemetry_topic, payload.c_str(), false);
  Serial.println("OK");
  delay(100);
  
}

void loop() {
  // nothing happens after setup finishes.
  clearDisplay();

  float t = dht.readTemperature();
  // fahrenheit = t * 1.8 + 32.0;
  float temp = t *1.8 + 32;
  totTemp += temp;
  delay(200);
  
  Serial.println("Temperature " + String(temp) ); 
  sendToDisplay(0, 8, "Temp & Humidity" ,STYLE_NORMAL);
  sendToDisplay(0, 16, "Temp:",STYLE_NORMAL);
  sendToDisplay(55, 16, String(temp),STYLE_NORMAL);

  float humidity = dht.readHumidity();
  totHumidity += humidity;
  delay(200);
  
  Serial.println("Humidity " + String(humidity) );
  sendToDisplay(0, 24, "Hum:" ,STYLE_NORMAL );
  sendToDisplay(55, 24, String(humidity),STYLE_NORMAL );
  delay(200);
  
  Serial.print("Interval :");
  Serial.print(IntervalCount);
  Serial.print(" of ");
  Serial.println(Interval);
    
  // send to Azure
  if (millis() > next_telemetry_send_time_ms)
  {
    // Check if connected, reconnect if needed.
    if(!mqtt_client.connected())
    {
      establishConnection(passData);
    }

    sendToDisplay(0 , 48, "Interval: " ,STYLE_NORMAL );
    sendToDisplay(55, 48, String(IntervalCount),STYLE_NORMAL );
    sendToDisplay(70, 48, "of",STYLE_NORMAL );
    sendToDisplay(90, 48, String(Interval),STYLE_NORMAL );
        
    sendToDisplay(0 , 56, "TimeDelay:",STYLE_NORMAL );
    sendToDisplay(60, 56, String(timedelay),STYLE_NORMAL );
    
    IntervalCount += 1;
    if(IntervalCount >= Interval)
    {   
        // get average readings
        totTemp = totTemp / Interval;
        totHumidity = totHumidity / Interval;
        totPressure = 0;
        totALtitude = 0;
        
        Serial.println("");
        Serial.print("Interval:");
        Serial.println("");
        Serial.println(Interval);
        Serial.println("");
        
        Serial.print("avg temp:");
        Serial.println(totTemp);
        
        Serial.print("avg Humidity:");
        Serial.println(totHumidity);

        Serial.print("avg Pressure:");
        Serial.println(totPressure);

        Serial.print("avg Altitude:");
        Serial.println(totALtitude);

        // move data to json for transport        
        String payload  = createJsonData(  passData[2] ,  totTemp,  totHumidity, totPressure, totALtitude, Interval,timedelay,getCurrentLocalTimeString() );
        Serial.print("Payload:");
        Serial.println(payload);
   
        sendTelemetry(payload);
        IntervalCount = 0;
        totTemp = 0;
        totHumidity = 0;
        totPressure = 0;
        totALtitude = 0;
    }
    
  }
  
  // MQTT loop must be called to process Device-to-Cloud and Cloud-to-Device.
  mqtt_client.loop();
  delay(timedelay);   // delay 60000 miliseconds = 60 sec
  
}
