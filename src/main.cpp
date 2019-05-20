#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "DHT.h" //DHT22

#include "individual_header.h"

/************************* DHT Setup *********************************/

#define DHTPIN1 5 // Digital pin connected to the DHT sensor
#define DHTPIN2 14 // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
DHT dht1(DHTPIN1, DHTTYPE);
DHT dht2(DHTPIN2, DHTTYPE);

WiFiClient client;

Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish hum1= Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/box1-hr1");
Adafruit_MQTT_Publish temp1 = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/box1-t1");
Adafruit_MQTT_Publish hum2= Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/box1-hr2");
Adafruit_MQTT_Publish temp2 = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/box1-t2");


void MQTT_connect();

void MQTT_publish1(float value1, float value2);
void MQTT_publish2(float value1, float value2);

void WiFi_connect() {
  // Connect to WiFi access point.
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  // WiFi fix: https://github.com/esp8266/Arduino/issues/2186
  WiFi.persistent(false);

  WiFi.forceSleepWake();
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("WiFi connected - ");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  
}

void loop() {
  Serial.begin(9600);
  delay(1);
  
  Serial.println();
  Serial.println();
  Serial.println("==============================================");
  Serial.println("//////////////////////////////////////////////");
  Serial.println();
  Serial.println(F("=============  Deepsleep DEMO ================"));

  WiFi_connect();

  dht1.begin();
  float hum1 = dht1.readHumidity();
  float temp1 = dht1.readTemperature();
  Serial.print(F("Humidity 1: "));
  Serial.print(hum1);
  Serial.print(F("%  Temperature 1: "));
  Serial.print(temp1);
  Serial.println(F("°C "));

  dht2.begin();
  float hum2 = dht2.readHumidity();
  float temp2 = dht2.readTemperature();

  Serial.print(F("Humidity 2: "));
  Serial.print(hum2);
  Serial.print(F("%  Temperature 2: "));
  Serial.print(temp2);
  Serial.println(F("°C "));

  MQTT_connect();

  // Check if the read not failed and then publish
  if (!(isnan(hum1) || isnan(temp1))) {
       MQTT_publish1(temp1, hum1);

       Serial.print(F("Humidity 1: "));
       Serial.print(hum1);
       Serial.print(F("%  Temperature 1: "));
       Serial.print(temp1);
       Serial.println(F("°C "));
    }

  if (!(isnan(hum2) || isnan(temp2))) {
       MQTT_publish2(temp2, hum2);

       Serial.print(F("Humidity 2: "));
       Serial.print(hum2);
       Serial.print(F("%  Temperature 2: "));
       Serial.print(temp2);
       Serial.println(F("°C "));
    }  

  mqtt.disconnect();


  WiFi.mode(WIFI_OFF);

  WiFi.forceSleepBegin();  // send wifi to sleep to reduce power consumption
  

  delay(1); // in ms

  Serial.println("Going into deep sleep for 6 minutes");
  Serial.println();
  Serial.println();
  Serial.println("************************************************");
  Serial.println();

  ESP.deepSleep(360000000); // in microseconds
  
  yield(); // pass control back to background processes to prepate sleep
}


// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
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
}

void MQTT_publish2(float value1,float value2){
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
}