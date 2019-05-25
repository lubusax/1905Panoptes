#include <FS.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "DHT.h" //DHT22

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40];
char mqtt_port[6] = "8080";
char blynk_token[34] = "YOUR_BLYNK_TOKEN";
char deepsleep_duration[3]; //in minutes
bool shouldSaveConfig = false; //flag for saving data

#include "individual_header.h"

#define DHTPIN1 5 // Digital pin connected to the DHT sensor
#define DHTPIN2 14 // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
#define SWITCHPIN 4 // Digital input from switch (normally open)

DHT dht1(DHTPIN1, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);

WiFiClient client;

Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish hum1= Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/box1-hr1");
Adafruit_MQTT_Publish temp1 = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/box1-t1");
Adafruit_MQTT_Publish hum2= Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/box1-hr2");
Adafruit_MQTT_Publish temp2 = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/box1-t2");


void MQTT_connect();
void SerialPrint_Welcome();
bool Is_switch_ON();
void SerialPrint_Measurement_DHT1(float temp1, float hum1);
void SerialPrint_Measurement_DHT2(float temp2, float hum2);
void MQTT_publish1(float value1, float value2);
void MQTT_publish2(float value1, float value2);
void WiFi_connect();
void SerialPrint_going_to_DeepSleep(String dur);

void saveConfigCallback () {  //callback notifying us of the need to save config
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void setup() {
 
}

void loop() {

  Serial.begin(9600);



  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(blynk_token, json["blynk_token"]);
          strcpy(deepsleep_duration, json["deepsleep_duration"]);

        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read


  
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 32);
  WiFiManagerParameter custom_ds_duration("duration", "deep sleep duration (min)", deepsleep_duration, 3);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setAPCallback(configModeCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_blynk_token);
  wifiManager.addParameter(&custom_ds_duration);
  
  if (!Is_switch_ON()) {
    Serial.println("disconnecting");

    WiFi.disconnect(true); //erases stored SSID and Password
    // LED ON
    wifiManager.startConfigPortal();

    //read updated parameters
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_port, custom_mqtt_port.getValue());
    strcpy(blynk_token, custom_blynk_token.getValue());
    strcpy(deepsleep_duration, custom_ds_duration.getValue()); 
  }

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  if (Is_switch_ON()) {
    wifiManager.setTimeout(1); // if switch is On, then no time for WiFiManager AP portal
  }


  

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("AutoConnectAP", "password")) {
    Serial.println("failed to connect and hit timeout");
  }

  if (WiFi.status() == WL_CONNECTED)   {
    //if you get here you have connected to the WiFi
    // LED OFF
    Serial.println("connected...yeey :)");

    //save the custom parameters to FS
    if (shouldSaveConfig) {
      Serial.println("saving config");
      DynamicJsonBuffer jsonBuffer;
      JsonObject& json = jsonBuffer.createObject();
      json["mqtt_server"] = mqtt_server;
      json["mqtt_port"] = mqtt_port;
      json["blynk_token"] = blynk_token;
      json["deepsleep_duration"] = deepsleep_duration;

      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        Serial.println("failed to open config file for writing");
      }

      json.printTo(Serial);
      json.printTo(configFile);
      configFile.close();
      //end save
    }

    Serial.println("local ip");
    Serial.println(WiFi.localIP());
    SerialPrint_Welcome();

    dht1.begin();
    float hum1 = dht1.readHumidity();
    float temp1 = dht1.readTemperature();
    SerialPrint_Measurement_DHT1(temp1,hum1);

    dht2.begin();
    float hum2 = dht2.readHumidity();
    float temp2 = dht2.readTemperature();
    SerialPrint_Measurement_DHT2(temp2,hum2);
    
    MQTT_connect();

    MQTT_publish1(temp1, hum1);

    MQTT_publish2(temp2, hum2);
      
    mqtt.disconnect();

  }

  WiFi.mode(WIFI_OFF);

  WiFi.forceSleepBegin();  // send wifi to sleep to reduce power consumption
  
  delay(1); // in ms

  SerialPrint_going_to_DeepSleep(deepsleep_duration);

  int duration = atoi(deepsleep_duration)*60000000;

  ESP.deepSleep(duration); // in microseconds
  
  yield(); // pass control back to background processes to prepate sleep
}

void SerialPrint_Welcome() {
  Serial.println();
  Serial.println();
  Serial.println("==============================================");
  Serial.println("//////////////////////////////////////////////");
  Serial.println();
  Serial.println(F("========  Measurement with DeepSleep ========="));
}

bool Is_switch_ON() {
  pinMode(SWITCHPIN, INPUT_PULLUP);
  bool buttonState = digitalRead(SWITCHPIN);
  Serial.println(buttonState ? "HIGH" : "LOW");
  return digitalRead(SWITCHPIN);
}

void SerialPrint_Measurement_DHT1(float temp1, float hum1) {
  Serial.print(F("Humidity 1: "));
  Serial.print(hum1);
  Serial.print(F("%  Temperature 1: "));
  Serial.print(temp1);
  Serial.println(F("°C "));
}

void SerialPrint_Measurement_DHT2(float temp2, float hum2) {
  Serial.print(F("Humidity 2: "));
  Serial.print(hum2);
  Serial.print(F("%  Temperature 2: "));
  Serial.print(temp2);
  Serial.println(F("°C "));
}

void MQTT_connect() {
  // Function to connect and reconnect as necessary to the MQTT server.
  // Should be called in the loop function and it will take care if connecting.
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
}

void MQTT_publish1(float value1,float value2){
  if (!(isnan(value2) || isnan(value1))) {

      if (! temp1.publish(value1)) {
        Serial.println(F("Failed to publish T1"));
      } else {
        Serial.println(F("T1 published!"));
      }

      if (! hum1.publish(value2)) {
        Serial.println(F("Failed to publish HR1"));
      } else {
        Serial.println(F("HR1 published!"));
      }

       Serial.print(F("Humidity 1: "));
       Serial.print(value2);
       Serial.print(F("%  Temperature 1: "));
       Serial.print(value1);
       Serial.println(F("°C "));
  }
}

void MQTT_publish2(float value1,float value2){
  if (!(isnan(value2) || isnan(value1))) {

      if (! temp2.publish(value1)) {
        Serial.println(F("Failed to publish T2"));
      } else {
        Serial.println(F("T2 published!"));
      }

      if (! hum2.publish(value2)) {
        Serial.println(F("Failed to publish HR2"));
      } else {
        Serial.println(F("HR2 published!"));
      }

       Serial.print(F("Humidity 2: "));
       Serial.print(value2);
       Serial.print(F("%  Temperature 2: "));
       Serial.print(value1);
       Serial.println(F("°C "));
    }  
}

void SerialPrint_going_to_DeepSleep(String dur) {
  Serial.print("Going into deep sleep for ");
  Serial.print(dur);
  Serial.println(" minutes");
  Serial.println();
  Serial.println();
  Serial.println("************************************************");
  Serial.println();
}