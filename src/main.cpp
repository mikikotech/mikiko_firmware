#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "time.h"
#include <Adafruit_Sensor.h>
#include <AsyncUDP.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>
#include "Update.h"
#include "EEPROM.h"
#include <Ticker.h>
#include <LiquidCrystal_I2C.h>
#include "OneButton.h"
#include "DHT.h"
#include <CronAlarms.h>
#include <math.h>

#define FIREBASE_PROJECT_ID "mikiko-c5ca4"
#define STORAGE_BUCKET_ID "gs://mikiko-c5ca4.appspot.com"
#define API_KEY "AIzaSyAMMrTWIU5gKeCDKwiLwO-7liVvfpT8u-M"
#define DATABASE_URL "https://mikiko-c5ca4-default-rtdb.firebaseio.com/"

// device info
#define DEVICE_EMAIL "mikikoMTH@mikiko.com"
#define DEVICE_PASS "mikikoMTH"
#define FIRMWARE_VERSION "1.0.1"

#define LENGTH(x) (strlen(x) + 1) // length of char string
#define EEPROM_SIZE 200           // EEPROM size

#define DHTPIN 25
#define DHTTYPE DHT21

#define out1 13 // relay
#define out2 18 // 18 // solenoid1
#define out3 2  // pin GPIO2 atau GPIO4  // solenoid2
#define out4 0  // pin GPIO0 atau GPIO16 // solenoid3
#define out5 5  // solenoid4
#define pinSensorHujan 26
#define phTanahPin A0
#define kelembabanTanahPin A1

WiFiClient espClient;
WiFiClient mikiko_client;
WiFiClient http_notif;
HTTPClient http;
PubSubClient client(espClient);

struct tm timeinfo;

String uniq_username = String("MIKIKO" + WiFi.macAddress());
String client_id;

const char *mqtt_broker = "broker.hivemq.com";
const char *mqtt_username = uniq_username.c_str();
const char *mqtt_password = "mikiko";
const int mqtt_port = 1883;

String api_endpoint;
CronId id;
DynamicJsonDocument schedule(1024);
DynamicJsonDocument http_data(128);
String string_http_data;

DHT dht(DHTPIN, DHTTYPE);
OneButton btn = OneButton(
    23,    // Input pin for the button
    false, // Button is active high
    false  // Disable internal pull-up resistor
);

Ticker ticker;
LiquidCrystal_I2C lcd(0x27, 20, 4);

WiFiUDP udp;

char udpbuf[3];
// char replyPacket[] = "MTH:5CH:MIKIKO";

String ssid; // string variable to store ssid
String pss;  // string variable to store password
String gmt;
String MACADD = WiFi.macAddress();

String topic1;
String topic2;
String topic3;
String topic4;
String topic5;
String rain_topic;
String fwVersion_topic;
String fwUpdate_topic;
String fwRespone_topic;
String schedule_topic;

uint64_t millisTime = 0;
uint64_t displayTime = 0;
long lastReconnectAttempt;

bool display1 = true;
bool display2 = false;
bool hujan = true;

float t, h, phTanahValue;
int kelembabanTanah;

String mask = "actions";
unsigned long epochTime;

void writeStringToFlash(const char *toStore, int startAddr)
{
  int i = 0;
  for (; i < LENGTH(toStore); i++)
  {
    EEPROM.write(startAddr + i, toStore[i]);
  }
  EEPROM.write(startAddr + i, '\0');
  EEPROM.commit();
}

String readStringFromFlash(int startAddr)
{
  char in[128]; // char array of size 128 for reading the stored data
  int i = 0;
  for (; i < 128; i++)
  {
    in[i] = EEPROM.read(startAddr + i);
  }
  return String(in);
}

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++)
  {
    if (data.charAt(i) == separator || i == maxIndex)
    {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void notif(uint8_t index, uint8_t state, uint8_t type)
{

  DynamicJsonDocument notif_data(128);
  String string_notif_data;

  api_endpoint = String("http://mikiko.herokuapp.com/notif/" + MACADD);

  notif_data["type"] = type; // 1 schedule // 2 sensor
  notif_data["index"] = index;
  notif_data["state"] = state;

  serializeJson(notif_data, string_notif_data);

  http.begin(http_notif, api_endpoint.c_str());
  http.addHeader("Content-Type", "application/json");

  if (http.POST(string_notif_data) > 0)
  {
    Serial.println(http.getString());
  }
  else
  {
    Serial.println("error notif");
  }

  http.end();
}

void out1_on()
{
  digitalWrite(out1, LOW);
  notif(0, 1, 1);
  client.publish(topic1.c_str(), "true", true);
}

void out1_off()
{
  digitalWrite(out1, HIGH);
  notif(0, 0, 1);
  client.publish(topic1.c_str(), "false", true);
}

void out2_on()
{
  digitalWrite(out2, HIGH);
  notif(1, 1, 1);
  client.publish(topic2.c_str(), "true", true);
}

void out2_off()
{
  digitalWrite(out2, LOW);
  notif(1, 0, 1);
  client.publish(topic2.c_str(), "false", true);
}

void out3_on()
{
  digitalWrite(out3, HIGH);
  notif(2, 1, 1);
  client.publish(topic3.c_str(), "true", true);
}

void out3_off()
{
  digitalWrite(out3, LOW);
  notif(2, 0, 1);
  client.publish(topic3.c_str(), "false", true);
}

void out4_on()
{
  digitalWrite(out4, HIGH);
  notif(3, 1, 1);
  client.publish(topic4.c_str(), "true", true);
}

void out4_off()
{
  digitalWrite(out4, LOW);
  notif(3, 0, 1);
  client.publish(topic4.c_str(), "false", true);
}

void out5_on()
{
  digitalWrite(out5, HIGH);
  notif(4, 1, 1);
  client.publish(topic5.c_str(), "true", true);
}

void out5_off()
{
  digitalWrite(out5, LOW);
  notif(4, 0, 1);
  client.publish(topic5.c_str(), "false", true);
}

void removeSchedule(CronID_t triggerCron)
{

  for (size_t i = 0; i < schedule.size(); i++)
  {
    String schedule_id = schedule[i]["id"];
    CronID_t cron_id = schedule[i]["cronId"];
    String cron_data = schedule[i]["data"];

    if (triggerCron == cron_id)
    {

      Serial.print("remove CronId = ");
      Serial.println(cron_id);

      DynamicJsonDocument http_data_remove(128);
      String string_http_data_remove;

      api_endpoint = String("http://mikiko.herokuapp.com/schedule/remove/" + MACADD);

      http_data_remove["id"] = schedule_id;
      http_data_remove["data"] = cron_data;

      serializeJson(http_data_remove, string_http_data_remove);

      http.begin(mikiko_client, api_endpoint.c_str());
      http.addHeader("Content-Type", "application/json");

      if (http.POST(string_http_data_remove) > 0)
      {
        Serial.print("HTTP Response code remove : ");
        Serial.println(http.getString());
      }
      else
      {
        Serial.print("Error code remove : ");
      }
      // Free resources
      http.end();

      // http_data_remove.clear();

      Cron.free(cron_id);
      cron_id = dtINVALID_ALARM_ID;
      schedule.remove(i);

      break;
    }
  }

  serializeJson(schedule, Serial);
}

void out1_on_once()
{
  out1_on();

  removeSchedule(Cron.getTriggeredCronId());
}

void out1_off_once()
{
  out1_off();

  removeSchedule(Cron.getTriggeredCronId());
}

void out2_on_once()
{
  out2_on();

  removeSchedule(Cron.getTriggeredCronId());
}

void out2_off_once()
{
  out2_off();

  removeSchedule(Cron.getTriggeredCronId());
}

void out3_on_once()
{
  out3_on();

  removeSchedule(Cron.getTriggeredCronId());
}

void out3_off_once()
{
  out3_off();

  removeSchedule(Cron.getTriggeredCronId());
}

void out4_on_once()
{
  out4_on();

  removeSchedule(Cron.getTriggeredCronId());
}

void out4_off_once()
{
  out4_off();

  removeSchedule(Cron.getTriggeredCronId());
}

void out5_on_once()
{
  out5_on();

  removeSchedule(Cron.getTriggeredCronId());
}

void out5_off_once()
{
  out5_off();

  removeSchedule(Cron.getTriggeredCronId());
}

void schedule_check()
{
  for (size_t j = 0; j < schedule.size(); j++)
  {
    String cron_data = schedule[j]["data"];
    String cron_string = getValue(cron_data, 58, 0);                // cron data
    String output = getValue(cron_data, 58, 1);                     // output
    bool state = getValue(cron_data, 58, 2) == "1" ? true : false;  // output state
    bool repeat = getValue(cron_data, 58, 3) == "1" ? true : false; // ?repeat
    bool status = getValue(cron_data, 58, 4) == "1" ? true : false; // ?schedule status

    if (output == "out1" && status == true)
    {
      if (repeat == true)
      {
        if (state == true)
        {
          id = Cron.create(cron_string.c_str(), out1_on_once, true);
          schedule[j]["cronId"] = id;
        }
        else
        {
          id = Cron.create(cron_string.c_str(), out1_off_once, true);
          schedule[j]["cronId"] = id;
        }
      }
      else
      {
        if (state == true)
        {
          id = Cron.create(cron_string.c_str(), out1_on, false);
          schedule[j]["cronId"] = id;
        }
        else
        {
          id = Cron.create(cron_string.c_str(), out1_off, false);
          schedule[j]["cronId"] = id;
        }
      }
    }
    else if (output == "out2" && status == true)
    {
      if (repeat == true)
      {
        if (state == true)
        {
          id = Cron.create(cron_string.c_str(), out2_on_once, true);
          schedule[j]["cronId"] = id;
        }
        else
        {
          id = Cron.create(cron_string.c_str(), out2_off_once, true);
          schedule[j]["cronId"] = id;
        }
      }
      else
      {
        if (state == true)
        {
          id = Cron.create(cron_string.c_str(), out2_on, false);
          schedule[j]["cronId"] = id;
        }
        else
        {
          id = Cron.create(cron_string.c_str(), out2_off, false);
          schedule[j]["cronId"] = id;
        }
      }
    }
    else if (output == "out3" && status == true)
    {
      if (repeat == true)
      {
        if (state == true)
        {
          id = Cron.create(cron_string.c_str(), out3_on_once, true);
          schedule[j]["cronId"] = id;
        }
        else
        {
          id = Cron.create(cron_string.c_str(), out3_off_once, true);
          schedule[j]["cronId"] = id;
        }
      }
      else
      {
        if (state == true)
        {
          id = Cron.create(cron_string.c_str(), out3_on, false);
          schedule[j]["cronId"] = id;
        }
        else
        {
          id = Cron.create(cron_string.c_str(), out3_off, false);
          schedule[j]["cronId"] = id;
        }
      }
    }
    else if (output == "out4" && status == true)
    {
      if (repeat == true)
      {
        if (state == true)
        {
          id = Cron.create(cron_string.c_str(), out4_on_once, true);
          schedule[j]["cronId"] = id;
        }
        else
        {
          id = Cron.create(cron_string.c_str(), out4_off_once, true);
          schedule[j]["cronId"] = id;
        }
      }
      else
      {
        if (state == true)
        {
          id = Cron.create(cron_string.c_str(), out4_on, false);
          schedule[j]["cronId"] = id;
        }
        else
        {
          id = Cron.create(cron_string.c_str(), out4_off, false);
          schedule[j]["cronId"] = id;
        }
      }
    }
    else if (output == "out5" && status == true)
    {
      if (repeat == true)
      {
        if (state == true)
        {
          id = Cron.create(cron_string.c_str(), out5_on_once, true);
          schedule[j]["cronId"] = id;
        }
        else
        {
          id = Cron.create(cron_string.c_str(), out5_off_once, true);
          schedule[j]["cronId"] = id;
        }
      }
      else
      {
        if (state == true)
        {
          id = Cron.create(cron_string.c_str(), out5_on, false);
          schedule[j]["cronId"] = id;
        }
        else
        {
          id = Cron.create(cron_string.c_str(), out5_off, false);
          schedule[j]["cronId"] = id;
        }
      }
    }
  }
}

void schedule_edit_check(DynamicJsonDocument schedule_data)
{
  String cron_data = schedule_data["data"];
  String cron_string = getValue(cron_data, 58, 0);                // cron data
  String output = getValue(cron_data, 58, 1);                     // output
  bool state = getValue(cron_data, 58, 2) == "1" ? true : false;  // output state
  bool repeat = getValue(cron_data, 58, 3) == "1" ? true : false; // ?repeat
  bool status = getValue(cron_data, 58, 4) == "1" ? true : false; // ?schedule status
  uint8_t schedule_size = schedule.size();

  if (output == "out1" && status == true)
  {
    if (repeat == true)
    {
      if (state == true)
      {
        id = Cron.create(cron_string.c_str(), out1_on_once, true);
        schedule[schedule_size]["cronId"] = id;
      }
      else
      {
        id = Cron.create(cron_string.c_str(), out1_off_once, true);
        schedule[schedule_size]["cronId"] = id;
      }
    }
    else
    {
      if (state == true)
      {
        id = Cron.create(cron_string.c_str(), out1_on, false);
        schedule[schedule_size]["cronId"] = id;
      }
      else
      {
        id = Cron.create(cron_string.c_str(), out1_off, false);
        schedule[schedule_size]["cronId"] = id;
      }
    }
  }
  else if (output == "out2" && status == true)
  {
    if (repeat == true)
    {
      if (state == true)
      {
        id = Cron.create(cron_string.c_str(), out2_on_once, true);
        schedule[schedule_size]["cronId"] = id;
      }
      else
      {
        id = Cron.create(cron_string.c_str(), out2_off_once, true);
        schedule[schedule_size]["cronId"] = id;
      }
    }
    else
    {
      if (state == true)
      {
        id = Cron.create(cron_string.c_str(), out2_on, false);
        schedule[schedule_size]["cronId"] = id;
      }
      else
      {
        id = Cron.create(cron_string.c_str(), out2_off, false);
        schedule[schedule_size]["cronId"] = id;
      }
    }
  }
  else if (output == "out3" && status == true)
  {
    if (repeat == true)
    {
      if (state == true)
      {
        id = Cron.create(cron_string.c_str(), out3_on_once, true);
        schedule[schedule_size]["cronId"] = id;
      }
      else
      {
        id = Cron.create(cron_string.c_str(), out3_off_once, true);
        schedule[schedule_size]["cronId"] = id;
      }
    }
    else
    {
      if (state == true)
      {
        id = Cron.create(cron_string.c_str(), out3_on, false);
        schedule[schedule_size]["cronId"] = id;
      }
      else
      {
        id = Cron.create(cron_string.c_str(), out3_off, false);
        schedule[schedule_size]["cronId"] = id;
      }
    }
  }
  else if (output == "out4" && status == true)
  {
    if (repeat == true)
    {
      if (state == true)
      {
        id = Cron.create(cron_string.c_str(), out4_on_once, true);
        schedule[schedule_size]["cronId"] = id;
      }
      else
      {
        id = Cron.create(cron_string.c_str(), out4_off_once, true);
        schedule[schedule_size]["cronId"] = id;
      }
    }
    else
    {
      if (state == true)
      {
        id = Cron.create(cron_string.c_str(), out4_on, false);
        schedule[schedule_size]["cronId"] = id;
      }
      else
      {
        id = Cron.create(cron_string.c_str(), out4_off, false);
        schedule[schedule_size]["cronId"] = id;
      }
    }
  }
  else if (output == "out5" && status == true)
  {
    if (repeat == true)
    {
      if (state == true)
      {
        id = Cron.create(cron_string.c_str(), out5_on_once, true);
        schedule[schedule_size]["cronId"] = id;
      }
      else
      {
        id = Cron.create(cron_string.c_str(), out5_off_once, true);
        schedule[schedule_size]["cronId"] = id;
      }
    }
    else
    {
      if (state == true)
      {
        id = Cron.create(cron_string.c_str(), out5_on, false);
        schedule[schedule_size]["cronId"] = id;
      }
      else
      {
        id = Cron.create(cron_string.c_str(), out5_off, false);
        schedule[schedule_size]["cronId"] = id;
      }
    }
  }

  schedule[schedule_size]["id"] = schedule_data["id"];
  schedule[schedule_size]["data"] = schedule_data["data"];

  serializeJson(schedule, Serial);
}

void btnLongPress()
{
  writeStringToFlash("", 0);
  writeStringToFlash("", 20);
  writeStringToFlash("", 40);
  ESP.restart();
  delay(500);
  ESP.restart();
}

void sensorRead()
{
  h = dht.readHumidity();
  t = dht.readTemperature();
  if (isnan(h) || isnan(t))
  {
    h = 0;
    t = 0;
  }

  // Serial.println(analogRead(kelembabanTanahPin));

  kelembabanTanah = map(analogRead(kelembabanTanahPin), 4095, 0, 0, 100);

  if (kelembabanTanah > 100)
  {
    kelembabanTanah = 100;
  }
  else if (kelembabanTanah < 0)
  {
    kelembabanTanah = 0;
  }

  int phADCval = analogRead(phTanahPin);
  phADCval = map(phADCval, 0, 4095, 4, 45);

  phTanahValue = (-0.0693 * phADCval) + 7.3855;
}

void mqtt_process(char *topic, byte *payload)
{

  String msg;
  String strTopic;

  strTopic = String((char *)topic);

  Serial.println(strTopic);

  if (strTopic == topic1)
  {

    msg = String((char *)payload);

    Serial.println(msg);

    if (msg == "true")
    {
      digitalWrite(out1, HIGH);
    }
    else
    {
      digitalWrite(out1, LOW);
    }
  }
  else if (strTopic == topic2)
  {

    msg = String((char *)payload);

    Serial.println(msg);

    if (msg == "true")
    {
      digitalWrite(out2, HIGH);
    }
    else
    {
      digitalWrite(out2, LOW);
    }
  }
  else if (strTopic == topic3)
  {

    msg = String((char *)payload);

    Serial.println(msg);

    if (msg == "true")
    {
      digitalWrite(out3, HIGH);
    }
    else
    {
      digitalWrite(out3, LOW);
    }
  }
  else if (strTopic == topic4)
  {
    msg = String((char *)payload);

    Serial.println(msg);

    if (msg == "true")
    {
      digitalWrite(out4, HIGH);
    }
    else
    {
      digitalWrite(out4, LOW);
    }
  }
  else if (strTopic == topic5)
  {
    msg = String((char *)payload);

    Serial.println(msg);

    if (msg == "true")
    {
      digitalWrite(out5, HIGH);
    }
    else
    {
      digitalWrite(out5, LOW);
    }
  }
  else if (strTopic == schedule_topic)
  {

    DynamicJsonDocument schedule_payload(512);

    msg = String((char *)payload);

    deserializeJson(schedule_payload, msg.c_str());

    serializeJson(schedule_payload, Serial);

    Serial.println("---------");

    if (schedule_payload["type"] == "11") // add schedule
    {
      schedule_edit_check(schedule_payload);
    }
    else if (schedule_payload["type"] == "22") // remove schedule
    {
      for (size_t i = 0; i < schedule.size(); i++)
      {
        if (schedule[i]["id"] == schedule_payload["id"])
        {
          CronID_t cron_id = schedule[i]["cronId"];

          Cron.free(cron_id);
          cron_id = dtINVALID_ALARM_ID;
          schedule.remove(i);

          Serial.printf("schedule index ke-%d dihapus", i);

          break;
        }
      }
    }
    else if (schedule_payload["type"] == "33") // edit schedule
    {
      for (size_t i = 0; i < schedule.size(); i++)
      {
        if (schedule[i]["id"] == schedule_payload["id"])
        {
          CronID_t cron_id = schedule[i]["cronId"];

          Cron.free(cron_id);
          cron_id = dtINVALID_ALARM_ID;
          schedule.remove(i);

          schedule_edit_check(schedule_payload);

          break;
        }
      }
    }
  }
  else if (strTopic == fwUpdate_topic)
  {
    msg = String((char *)payload);
  }
}

void reconnect_to_mqtt()
{
  Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
  if (client.connect(client_id.c_str(), mqtt_username, mqtt_password, MACADD.c_str(), 2, true, "false"))
  {

    client.subscribe(topic1.c_str());
    client.subscribe(topic2.c_str());
    client.subscribe(topic3.c_str());
    client.subscribe(topic4.c_str());
    client.subscribe(topic5.c_str());

    client.subscribe(schedule_topic.c_str());

    client.subscribe(fwUpdate_topic.c_str());
    client.publish(fwVersion_topic.c_str(), FIRMWARE_VERSION, true);

    client.publish(MACADD.c_str(), "true", true);
  }
  else
  {
    Serial.print("failed with state ");
    Serial.print(client.state());
    delay(2000);
  }
}

void callback(char *topic, byte *payload, unsigned int length)
{
  payload[length] = '\0';
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);

  Serial.print("Message:");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }

  Serial.println();
  Serial.println("-----------------------");

  mqtt_process(topic, payload);
}

// ==================================================================

void setup()
{
  Serial.begin(115200);

  if (!EEPROM.begin(EEPROM_SIZE))
  { // Init EEPROM
    Serial.println("failed to init EEPROM");
    delay(1000);
  }
  else
  {
    ssid = readStringFromFlash(0);
    pss = readStringFromFlash(20);
    gmt = readStringFromFlash(40);
  }

  MACADD = String(WiFi.macAddress());

  MACADD = getValue(MACADD, 58, 0) + getValue(MACADD, 58, 1) + getValue(MACADD, 58, 2) + getValue(MACADD, 58, 3) + getValue(MACADD, 58, 4) + getValue(MACADD, 58, 5);
  MACADD.toLowerCase();

  Serial.println(MACADD);

  String str_reply = String("SON:5CH:" + String(lround(ESP.getEfuseMac() / 1234)) + ":MIKIKO");
  char replyPacket[str_reply.length() + 1];

  api_endpoint = String("http://mikiko.herokuapp.com/schedule/getall/" + MACADD);

  topic1 = String("/" + String(MACADD) + "/data/btn1");
  topic2 = String("/" + String(MACADD) + "/data/btn2");
  topic3 = String("/" + String(MACADD) + "/data/btn3");
  topic4 = String("/" + String(MACADD) + "/data/btn4");
  topic5 = String("/" + String(MACADD) + "/data/btn5");
  rain_topic = String("/" + MACADD + "/data/weather");

  fwVersion_topic = String("/" + MACADD + "/data/firmwareversion");
  fwUpdate_topic = String("/" + String(MACADD) + "/data/ota");
  fwRespone_topic = String("/" + String(MACADD) + "/data/otarespone");

  schedule_topic = String("/" + String(MACADD) + "/data/schedule");

  strcpy(replyPacket, str_reply.c_str());

  lcd.begin();
  lcd.backlight();
  dht.begin();
  btn.tick();

  pinMode(pinSensorHujan, INPUT);
  pinMode(DHTPIN, INPUT);
  pinMode(kelembabanTanahPin, INPUT);
  pinMode(23, INPUT);
  pinMode(out1, OUTPUT);
  pinMode(out2, OUTPUT);
  pinMode(out3, OUTPUT);
  pinMode(out4, OUTPUT);
  pinMode(out5, OUTPUT);

  digitalWrite(out1, HIGH);
  digitalWrite(out2, LOW);
  digitalWrite(out3, LOW);
  digitalWrite(out4, LOW);
  digitalWrite(out5, LOW);

  btn.attachLongPressStart(btnLongPress);
  btn.setPressTicks(2000);

  // writeStringToFlash("Mikikotech", 0);
  // writeStringToFlash("6jt/bulan", 20);
  // writeStringToFlash("8", 40);

  if (ssid.length() > 0 && pss.length() > 0)
  {
    WiFi.begin(ssid.c_str(), pss.c_str());

    lcd.setCursor(0, 1);
    lcd.print("Trying Connect Wifi");

    Serial.print("Connecting to WiFi ..");
    while (WiFi.status() != WL_CONNECTED)
    {
      btn.tick();
      delay(1);
    }
    Serial.println(WiFi.localIP());
  }
  else
  {
    WiFi.mode(WIFI_STA);

    WiFi.beginSmartConfig();

    udp.begin(2255);

    lcd.setCursor(0, 1);
    lcd.print("    Waiting WiFi   ");

    while (!WiFi.smartConfigDone())
    {
      // delayMicroseconds(5);
      delay(1);
      btn.tick();
      // Serial.print(".");
    }

    while (WiFi.status() != WL_CONNECTED)
    {
      // delayMicroseconds(5);
      delay(1);
      btn.tick();
      // Serial.print(".");
    }

    while (true)
    {
      btn.tick();
      if (udp.parsePacket())
      {
        udp.read(udpbuf, 3);
        Serial.print("message = ");
        Serial.print(udpbuf);
        // writeStringToFlash(udpbuf, 40);
        gmt = udpbuf;
        Serial.print(", from =");
        Serial.print(udp.remoteIP());

        udp.beginPacket(udp.remoteIP(), 2255);
        int i = 0;
        while (replyPacket[i] != 0)
          udp.write((uint8_t)replyPacket[i++]);
        Serial.print("udp send = ");
        Serial.println(replyPacket);
        udp.endPacket();
        udp.flush();

        break;
      }
    }

    ssid = WiFi.SSID();
    pss = WiFi.psk();

    writeStringToFlash(ssid.c_str(), 0);
    writeStringToFlash(pss.c_str(), 20);
  }

  lcd.clear();
  lcd.setCursor(3, 1);
  lcd.print("Connecting to ");
  lcd.setCursor((20 - ssid.length()) / 2, 2);
  lcd.print(ssid);

  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  configTime(0, gmt.toInt() * 3600, "pool.ntp.org");

  while (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    // return;
  }

  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);

  while (!client.connected())
  {
    reconnect_to_mqtt();
  }

  http.begin(mikiko_client, api_endpoint.c_str());

  if (http.GET() > 0)
  {
    Serial.print("HTTP Response code: ");
    deserializeJson(schedule, http.getString().c_str());

    if (schedule.size() > 0)
    {
      schedule_check();
    }
  }
  else
  {
    Serial.print("Error code: ");
    // Serial.println(http.GET());
  }

  http.end();

  delay(2000);

  lcd.clear();
  lcd.setCursor(4, 1);
  lcd.print("Connected !!");
  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" MIKIKO TECHNOLOGY ");
  lcd.setCursor(0, 1);
  lcd.print("--------------------");
}

void sensorDisplay()
{
  if (millisTime - displayTime > 4000 && display1)
  {
    display2 = true;

    lcd.setCursor(0, 2);
    lcd.print("Temp = ");
    lcd.print(t);
    lcd.print((char)223);
    lcd.print("C    ");
    lcd.setCursor(0, 3);
    lcd.print("Humidity = ");
    lcd.print(h);
    lcd.print(" % ");

    displayTime = millisTime;
    display1 = false;
  }

  if (millisTime - displayTime > 4000 && display2)
  {
    display1 = true;

    lcd.setCursor(0, 2);
    lcd.print("Ph tanah = ");
    lcd.print(String(String(phTanahValue) + "   "));
    lcd.setCursor(0, 3);
    lcd.print("Soil RH = ");
    lcd.print(kelembabanTanah);
    lcd.print(" %      ");

    displayTime = millisTime;
    display2 = false;
  }
}

void rainSensor()
{
  if (!digitalRead(pinSensorHujan) && hujan)
  {
    client.publish(rain_topic.c_str(), "true", false);
    hujan = false;
  }

  if (digitalRead(pinSensorHujan) && !hujan)
  {
    client.publish(rain_topic.c_str(), "false", false);
    hujan = true;
  }
}

unsigned long getTime()
{
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    // Serial.println("Failed to obtain time");
    return (0);
  }
  time(&now);
  return now;
}

void loop()
{
  getLocalTime(&timeinfo);

  epochTime = getTime();

  millisTime = millis();

  Cron.delay();

  btn.tick();

  if (!client.connected() || WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    long now = millis();
    if (now - lastReconnectAttempt > 5000)
    {
      reconnect_to_mqtt();
      // Attempt to reconnect
      lastReconnectAttempt = now;
    }
  }
  else
  {
    client.loop();
  }

  sensorRead();
  // rainSensor();
  sensorDisplay();

  if (timeinfo.tm_sec % 20 == 0)
  {

    DynamicJsonDocument sensor_data_set(128);
    String string_sensor_data_set;

    sensor_data_set["humi"] = h;
    sensor_data_set["temp"] = t;
    sensor_data_set["soil"] = kelembabanTanah;
    sensor_data_set["ph"] = phTanahValue;

    serializeJson(sensor_data_set, string_sensor_data_set);

    api_endpoint = String("http://mikiko.herokuapp.com/sensor/set/" + MACADD);

    http.begin(mikiko_client, api_endpoint.c_str());
    http.addHeader("Content-Type", "application/json");

    if (http.POST(string_sensor_data_set) > 0)
    {
      Serial.print("HTTP Response code: ");
      Serial.println(http.getString());
    }
    else
    {
      Serial.print("Error code: ");
    }

    http.end();
  }

  if (timeinfo.tm_min % 30 == 0 && timeinfo.tm_sec == 0)
  {

    DynamicJsonDocument sensor_data(256);
    String string_sensor_data;

    sensor_data["humi"] = h;
    sensor_data["temp"] = t;
    sensor_data["soil"] = kelembabanTanah;
    sensor_data["ph"] = phTanahValue;
    sensor_data["time"] = epochTime;

    serializeJson(sensor_data, string_sensor_data);

    api_endpoint = String("http://mikiko.herokuapp.com/sensor/add/" + MACADD);

    http.begin(mikiko_client, api_endpoint.c_str());
    http.addHeader("Content-Type", "application/json");

    if (http.POST(string_sensor_data) > 0)
    {
      Serial.print("HTTP Response code: ");
      Serial.println(http.getString());
    }
    else
    {
      Serial.print("Error code: ");
    }

    http.end();
  }

  delayMicroseconds(5);
}