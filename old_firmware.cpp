#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#include <Adafruit_Sensor.h>
#include <AsyncUDP.h>
#include <EspMQTTClient.h>
#include <WiFiUdp.h>
#include "Update.h"
#include "EEPROM.h"
#include <Ticker.h>
#include <LiquidCrystal_I2C.h>
#include "OneButton.h"
#include "DHT.h"
#include <NTPClient.h>

// #define WIFI_SSID "Mikikotech"
// #define WIFI_PASSWORD "6jt/bulan"

// #define FIREBASE_PROJECT_ID "mikiko-c5ca4"
// #define STORAGE_BUCKET_ID "mikiko-c5ca4.appspot.com"
// #define API_KEY "AIzaSyAMMrTWIU5gKeCDKwiLwO-7liVvfpT8u-M"
// #define DATABASE_URL "https://mikiko-c5ca4-default-rtdb.firebaseio.com/"

#define FIREBASE_PROJECT_ID "mikiko-c5ca4"
#define STORAGE_BUCKET_ID "gs://mikiko-c5ca4.appspot.com"
#define API_KEY "AIzaSyAMMrTWIU5gKeCDKwiLwO-7liVvfpT8u-M"
#define DATABASE_URL "https://mikiko-c5ca4-default-rtdb.firebaseio.com/"

// device info
#define DEVICE_EMAIL "mikikoMTH@mikiko.com"
#define DEVICE_PASS "mikikoMTH"
#define FIRMWARE_VERSION "0.0.1"

#define LENGTH(x) (strlen(x) + 1) // length of char string
#define EEPROM_SIZE 200           // EEPROM size

#define DHTPIN 25
#define DHTTYPE DHT21

#define out1 13 // relay
#define out2 18 // solenoid1
#define out3 4  // pin GPIO2 atau GPIO4  // solenoid2
#define out4 16 // pin GPIO0 atau GPIO16 // solenoid3
#define out5 5  // solenoid4
#define pinSensorHujan 26
#define phTanahPin A0
#define kelembabanTanahPin A1

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
FirebaseJson json;

DHT dht(DHTPIN, DHTTYPE);
OneButton btn = OneButton(
    23,    // Input pin for the button
    false, // Button is active high
    false  // Disable internal pull-up resistor
);

Ticker ticker;
LiquidCrystal_I2C lcd(0x27, 20, 4);

WiFiUDP udp;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
// HTTPClient http;

DynamicJsonDocument actions(2048);
DynamicJsonDocument schedule(2048);

TaskHandle_t task1_handle = NULL;
TaskHandle_t task2_handle = NULL;
TaskHandle_t task3_handle = NULL;
TaskHandle_t task4_handle = NULL;
TaskHandle_t task5_handle = NULL;

unsigned long sendDataPrevMillis = 0;
int count = 0;
bool signupOK = false;

char udpbuf[255];
char replyPacket[] = "MTH:5CH:MIKIKO";

String ssid; // string variable to store ssid
String pss;  // string variable to store password
String docPathId;
String MACADD = WiFi.macAddress();

// String sendTime;
uint8_t days;

uint64_t millisTime = 0;
uint64_t displayTime = 0;
unsigned long int sensorTime = 0;
unsigned long int rtdbTime = 0;

bool udpmsg = true;
bool wificheck = true;
bool display1 = true;
bool display2 = false;
bool hujan = true;
bool timmerCheck1 = true;
bool timmerCheck2 = true;
bool timmerCheck3 = true;
bool timmerCheck4 = true;
bool timmerCheck5 = true;

int duration1;
int duration2;
int duration3;
int duration4;
int duration5;
float t, h, phTanahValue;
int kelembabanTanah;

String documentPath;
String mask = "actions";
String hour, minutes, sendTime;

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

void btnLongPress()
{
    writeStringToFlash("", 0);  // Reset the SSID
    writeStringToFlash("", 40); // Reset the Password
    writeStringToFlash("", 80); // Reset docPathId
    Serial.println("Wifi credentials erased");
    Serial.println("Restarting the ESP");
    delay(500);
    ESP.restart();
}

EspMQTTClient client(
    ssid.c_str(),
    pss.c_str(),
    "broker.hivemq.com",
    MACADD.c_str(),
    1883);

void timmerOne(void *parameter)
{
    for (;;)
    { // infinite loop

        digitalWrite(out1, LOW);
        vTaskDelay((duration1 * 60 * 1000) / portTICK_PERIOD_MS);
        digitalWrite(out1, HIGH);
        timmerCheck1 = true;

        vTaskSuspend(task1_handle);
    }
}

void timmerTwo(void *parameter)
{
    for (;;)
    {

        digitalWrite(out2, LOW);
        vTaskDelay((duration2 * 60 * 1000) / portTICK_PERIOD_MS);
        digitalWrite(out2, HIGH);
        timmerCheck2 = true;

        vTaskSuspend(task2_handle);
    }
}

void timmerThree(void *parameter)
{
    for (;;)
    {

        digitalWrite(out3, LOW);
        vTaskDelay((duration3 * 60 * 1000) / portTICK_PERIOD_MS);
        digitalWrite(out3, HIGH);
        timmerCheck3 = true;

        vTaskSuspend(task3_handle);
    }
}

void timmerFour(void *parameter)
{
    for (;;)
    {

        digitalWrite(out4, LOW);
        vTaskDelay((duration4 * 60 * 1000) / portTICK_PERIOD_MS);
        digitalWrite(out4, HIGH);
        timmerCheck4 = true;

        vTaskSuspend(task4_handle);
    }
}

void timmerFive(void *parameter)
{
    for (;;)
    {

        digitalWrite(out5, LOW);
        vTaskDelay((duration5 * 60 * 1000) / portTICK_PERIOD_MS);
        digitalWrite(out5, HIGH);
        timmerCheck5 = true;

        vTaskSuspend(task5_handle);
    }
}

void connectToWifi()
{

    if (!EEPROM.begin(EEPROM_SIZE))
    { // Init EEPROM
        Serial.println("failed to init EEPROM");
        delay(1000);
    }
    else
    {
        ssid = readStringFromFlash(0); // Read SSID stored at address 0
        Serial.print("SSID = ");
        Serial.println(ssid);
        pss = readStringFromFlash(40); // Read Password stored at address 40
        Serial.print("psss = ");
        Serial.println(pss);
        docPathId = readStringFromFlash(80); // Read Password stored at address 80
        Serial.print("docpathid = ");
        Serial.println(docPathId);
    }

    writeStringToFlash("", 0);
    writeStringToFlash("", 40);
    writeStringToFlash("", 80);

    // ssid = "Wifi saya";
    // pss = "1sampai9";

    WiFi.mode(WIFI_AP_STA);

    uint8_t cnt = 0;

    if (ssid.length() > 0 && pss.length() > 0)
    {
        WiFi.begin(ssid.c_str(), pss.c_str());

        while (WiFi.status() != WL_CONNECTED && wificheck == true)
        {
            btn.tick();
            lcd.setCursor(0, 1);
            lcd.print("Trying Connect Wifi");
            Serial.print("-");
            if (cnt > 5000)
            {
                wificheck = false;
                writeStringToFlash("", 0);  // Reset the SSID
                writeStringToFlash("", 40); // Reset the Password
                writeStringToFlash("", 80); // Reset docPathId
                break;
            }
            cnt++;
        }
        lcd.clear();
        lcd.setCursor(3, 1);
        lcd.print("Connecting to ");
        lcd.setCursor((20 - ssid.length()) / 2, 2);
        lcd.print(ssid);
    }

    if (WiFi.status() != WL_CONNECTED) // if WiFi is not connected
    {
        WiFi.beginSmartConfig();

        udp.begin(2255);

        while (WiFi.status() != WL_CONNECTED)
        {
            btn.tick();
            Serial.print(".");
            lcd.setCursor(0, 1);
            lcd.print("    Waiting WiFi   ");
        }

        while (udpmsg)
        {
            btn.tick();
            if (udp.parsePacket())
            {
                udp.read(udpbuf, 255);
                Serial.print("message = ");
                Serial.print(udpbuf);
                writeStringToFlash(udpbuf, 80); // storing docPathId at address 40
                docPathId = String(udpbuf);
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
                udpmsg = false;
            }
        }

        ssid = WiFi.SSID();
        pss = WiFi.psk();

        lcd.clear();
        lcd.setCursor(3, 1);
        lcd.print("Connecting to ");
        lcd.setCursor((20 - ssid.length()) / 2, 2);
        lcd.print(ssid);

        WiFi.stopSmartConfig();

        Serial.println("WiFi Connected.");

        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());

        writeStringToFlash(ssid.c_str(), 0); // storing ssid at address 0
        writeStringToFlash(pss.c_str(), 40); // storing pss at address 40
    }

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

void sensorRead()
{
    h = dht.readHumidity();
    t = dht.readTemperature();
    if (isnan(h) || isnan(t))
    {
        h = 0;
        t = 0;
    }

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

void setup()
{
    Serial.begin(115200);

    xTaskCreatePinnedToCore(
        timmerOne,     // Function that should be called
        "timmer 1",    // Name of the task (for debugging)
        1000,          // Stack size (bytes)
        NULL,          // Parameter to pass
        1,             // Task priority
        &task1_handle, // Task handle
        0);

    xTaskCreatePinnedToCore(
        timmerTwo,     // Function that should be called
        "timmer 2",    // Name of the task (for debugging)
        1000,          // Stack size (bytes)
        NULL,          // Parameter to pass
        1,             // Task priority
        &task2_handle, // Task handle
        0);

    xTaskCreatePinnedToCore(
        timmerThree,   // Function that should be called
        "timmer 3",    // Name of the task (for debugging)
        1000,          // Stack size (bytes)
        NULL,          // Parameter to pass
        1,             // Task priority
        &task3_handle, // Task handle
        0);

    xTaskCreatePinnedToCore(
        timmerFour,    // Function that should be called
        "timmer 4",    // Name of the task (for debugging)
        1000,          // Stack size (bytes)
        NULL,          // Parameter to pass
        1,             // Task priority
        &task4_handle, // Task handle
        0);

    xTaskCreatePinnedToCore(
        timmerFive,    // Function that should be called
        "timmer 5",    // Name of the task (for debugging)
        1000,          // Stack size (bytes)
        NULL,          // Parameter to pass
        1,             // Task priority
        &task5_handle, // Task handle
        0);

    vTaskSuspend(task1_handle);
    vTaskSuspend(task2_handle);
    vTaskSuspend(task3_handle);
    vTaskSuspend(task4_handle);
    vTaskSuspend(task5_handle);

    MACADD = String(WiFi.macAddress());

    MACADD = getValue(MACADD, 58, 0) + getValue(MACADD, 58, 1) + getValue(MACADD, 58, 2) + getValue(MACADD, 58, 3) + getValue(MACADD, 58, 4) + getValue(MACADD, 58, 5);
    MACADD.toLowerCase();

    Serial.println(MACADD);

    lcd.begin();
    lcd.backlight();
    dht.begin();
    btn.tick();

    pinMode(BUILTIN_LED, OUTPUT);
    pinMode(pinSensorHujan, INPUT);
    pinMode(23, INPUT);
    pinMode(out2, OUTPUT);
    pinMode(out3, OUTPUT);
    pinMode(out4, OUTPUT);
    pinMode(out5, OUTPUT);
    pinMode(out1, OUTPUT);

    digitalWrite(out1, HIGH);

    btn.attachLongPressStart(btnLongPress);
    btn.setPressTicks(2000);

    connectToWifi();

    client.enableDebuggingMessages();
    client.enableHTTPWebUpdater(MACADD.c_str(), "");
    client.enableOTA();
    client.enableLastWillMessage(MACADD.c_str(), "false", true);

    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;

    timeClient.begin();
    timeClient.setTimeOffset(8 * 3600);
    timeClient.forceUpdate();

    // documentPath = String(docPathId + "/" + MACADD);

    documentPath = String("devices/" + MACADD);

    /* Sign up */
    // if (Firebase.signUp(&config, &auth, "mikikotech@gmail.com", "Dalungww23"))
    // {
    //   signupOK = true;
    // }
    // else
    // {
    //   Serial.printf("%s\n", config.signer.signupError.message.c_str());
    // }

    auth.user.email = DEVICE_EMAIL;
    auth.user.password = DEVICE_PASS;

    config.token_status_callback = tokenStatusCallback;

    fbdo.setResponseSize(4095);
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    hour = String(timeClient.getHours());
    minutes = String(timeClient.getMinutes());

    if (hour.length() == 1)
    {
        hour = String("0" + hour);
    }

    if (minutes.length() == 1)
    {
        minutes = String("0" + minutes);
    }

    sendTime = String(hour + ":" + minutes);

    sensorRead();

    json.set("/temp", t);
    json.set("/hum", h);
    json.set("/soil", kelembabanTanah);
    json.set("/ph", phTanahValue);
    json.set("/time", sendTime);

    Firebase.RTDB.pushJSON(&fbdo, "/" + MACADD + "/Sensor", &json);

    Firebase.RTDB.setInt(&fbdo, "/" + MACADD + "/data/temp", t);
    Firebase.RTDB.setInt(&fbdo, "/" + MACADD + "/data/humi", h);
    Firebase.RTDB.setInt(&fbdo, "/" + MACADD + "/data/soil", kelembabanTanah);
    Firebase.RTDB.setInt(&fbdo, "/" + MACADD + "/data/ph", phTanahValue);
}

void fcsDownloadCallback(FCS_DownloadStatusInfo info)
{
    if (info.status == fb_esp_fcs_download_status_init)
    {
        Serial.printf("Downloading firmware %s (%d)\n", info.remoteFileName.c_str(), info.fileSize);
        lcd.clear();
        lcd.setCursor(0, 1);
        lcd.print("Downloading firmware");
        delay(1000);
    }
    else if (info.status == fb_esp_fcs_download_status_download)
    {
        Serial.printf("Downloaded %d%s\n", (int)info.progress, "%");
        lcd.clear();
        lcd.setCursor(3, 1);
        lcd.printf("Downloaded %d%s\n", (int)info.progress, "%");
    }
    else if (info.status == fb_esp_fcs_download_status_complete)
    {
        lcd.clear();
        lcd.setCursor(0, 1);
        lcd.print("installing firmware!");
        Serial.println("Update firmware completed.");
        Serial.println();
        Serial.println("Restarting...\n\n");
        delay(3000);
        lcd.clear();
        lcd.setCursor(8, 1);
        lcd.print("Done");
        delay(1000);
        ESP.restart();
    }
    else if (info.status == fb_esp_fcs_download_status_error)
    {
        Serial.printf("Download firmware failed, %s\n", info.errorMsg.c_str());
    }
}

void onConnectionEstablished()
{
    client.publish(String("/" + MACADD + "/data/weather"), "false", false);
    client.publish(String("/" + MACADD + "/data/firmwareversion"), FIRMWARE_VERSION, true);
    client.publish(MACADD.c_str(), "true", true);

    client.subscribe(String("/" + MACADD + "/data/btnone"), [](const String &payload)
                     {
                     Serial.println(payload);

                     if (payload == "true")
                     {
                       digitalWrite(out1, LOW);
                     }
                     else
                     {
                       digitalWrite(out1, HIGH);
                     } });
    client.subscribe(String("/" + MACADD + "/data/btntwo"), [](const String &payload)
                     {
                     Serial.println(payload);

                     if (payload == "true")
                     {
                       digitalWrite(out2, LOW);
                     }
                     else
                     {
                       digitalWrite(out2, HIGH);
                     } });

    client.subscribe(String("/" + MACADD + "/data/btnthree"), [](const String &payload)
                     {
                     Serial.println(payload);

                     if (payload == "true")
                     {
                       digitalWrite(out3, LOW);
                     }
                     else
                     {
                       digitalWrite(out3, HIGH);
                     } });

    client.subscribe(String("/" + MACADD + "/data/btnfour"), [](const String &payload)
                     {
                     Serial.println(payload);

                     if (payload == "true")
                     {
                       digitalWrite(out4, LOW);
                     }
                     else
                     {
                       digitalWrite(out4, HIGH);
                     } });

    client.subscribe(String("/" + MACADD + "/data/btnfive"), [](const String &payload)
                     {
                     Serial.println(payload);

                     if (payload == "true")
                     {
                       digitalWrite(out5, LOW);
                     }
                     else
                     {
                       digitalWrite(out5, HIGH);
                     } });

    client.subscribe(String("/" + MACADD + "/data/ota"), [](const String &payload)
                     {if(payload == "true"){
                     if (!Firebase.Storage.downloadOTA(&fbdo, STORAGE_BUCKET_ID, "MTH/firmware.bin", fcsDownloadCallback))
                        Serial.println(fbdo.errorReason());
                    } });

    client.subscribe(String("/" + MACADD + "/data/actions"), [](const String &payload)
                     {if(payload == "true"){
                    // get data from firestore
                    if (Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), "actions")){
                      Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
                      deserializeJson(actions, fbdo.payload());
                      actions["actions"] = actions["fields"]["actions"]["arrayValue"]["values"];
                    }else{
                      Serial.println(fbdo.errorReason());
                    }
                    } });

    client.subscribe(String("/" + MACADD + "/data/schedule"), [](const String &payload)
                     {if(payload == "true"){
                    // get data from firestore
                    if (Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), "schedule")){
                      Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
                      deserializeJson(schedule, fbdo.payload());
                      schedule["schedule"] = schedule["fields"]["schedule"]["arrayValue"]["values"];
                    }else{
                      Serial.println(fbdo.errorReason());
                    }
                    } });
}

void scheduleAndAction()
{
    // schedule check======================================================================

    for (int j = 0; j < schedule["schedule"].size(); j++)
    {
        String output = schedule["schedule"][j]["mapValue"]["fields"]["output"]["stringValue"];
        // Serial.print("output = ");
        // Serial.println(output);
        if (output == "out1")
        {
            if (timmerCheck1 == true)
            {
                bool status = schedule["schedule"][j]["mapValue"]["fields"]["status"]["booleanValue"];
                String time = schedule["schedule"][j]["mapValue"]["fields"]["time"]["stringValue"];
                uint8_t day = schedule["schedule"][j]["mapValue"]["fields"]["every"]["integerValue"];
                if (time == sendTime && status == true && days == day)
                {
                    duration1 = schedule["schedule"][j]["mapValue"]["fields"]["duration"]["integerValue"];
                    vTaskResume(task1_handle);
                    timmerCheck1 = false;
                }
                else if (time == sendTime && status == true && day > 7)
                {
                    duration1 = schedule["schedule"][j]["mapValue"]["fields"]["duration"]["integerValue"];
                    vTaskResume(task1_handle);
                    timmerCheck1 = false;
                }
            }
        }
        else if (output == "out2")
        {
            if (timmerCheck2 == true)
            {
                bool status = schedule["schedule"][j]["mapValue"]["fields"]["status"]["booleanValue"];
                String time = schedule["schedule"][j]["mapValue"]["fields"]["time"]["stringValue"];
                uint8_t day = schedule["schedule"][j]["mapValue"]["fields"]["every"]["integerValue"];
                if (time == sendTime && status == true && days == day)
                {
                    duration2 = schedule["schedule"][j]["mapValue"]["fields"]["duration"]["integerValue"];
                    vTaskResume(task2_handle);
                    timmerCheck2 = false;
                }
                else if (time == sendTime && status == true && day > 7)
                {
                    duration2 = schedule["schedule"][j]["mapValue"]["fields"]["duration"]["integerValue"];
                    vTaskResume(task2_handle);
                    timmerCheck2 = false;
                }
            }
        }
        else if (output == "out3")
        {
            if (timmerCheck3 == true)
            {
                bool status = schedule["schedule"][j]["mapValue"]["fields"]["status"]["booleanValue"];
                String time = schedule["schedule"][j]["mapValue"]["fields"]["time"]["stringValue"];
                uint8_t day = schedule["schedule"][j]["mapValue"]["fields"]["every"]["integerValue"];
                if (time == sendTime && status == true && days == day)
                {
                    duration3 = schedule["schedule"][j]["mapValue"]["fields"]["duration"]["integerValue"];
                    vTaskResume(task3_handle);
                    timmerCheck3 = false;
                }
                else if (time == sendTime && status == true && day > 7)
                {
                    duration3 = schedule["schedule"][j]["mapValue"]["fields"]["duration"]["integerValue"];
                    vTaskResume(task3_handle);
                    timmerCheck3 = false;
                }
            }
        }
        else if (output == "out4")
        {
            if (timmerCheck4 == true)
            {
                bool status = schedule["schedule"][j]["mapValue"]["fields"]["status"]["booleanValue"];
                String time = schedule["schedule"][j]["mapValue"]["fields"]["time"]["stringValue"];
                uint8_t day = schedule["schedule"][j]["mapValue"]["fields"]["every"]["integerValue"];
                // Serial.print("time = ");
                // Serial.println(time);
                if (time == sendTime && status == true && days == day)
                {
                    duration4 = schedule["schedule"][j]["mapValue"]["fields"]["duration"]["integerValue"];
                    vTaskResume(task4_handle);
                    timmerCheck4 = false;
                }
                else if (time == sendTime && status == true && day > 7)
                {
                    duration4 = schedule["schedule"][j]["mapValue"]["fields"]["duration"]["integerValue"];
                    vTaskResume(task4_handle);
                    timmerCheck4 = false;
                }
            }
        }
        else if (output == "out5")
        {
            if (timmerCheck5 == true)
            {
                bool status = schedule["schedule"][j]["mapValue"]["fields"]["status"]["booleanValue"];
                String time = schedule["schedule"][j]["mapValue"]["fields"]["time"]["stringValue"];
                uint8_t day = schedule["schedule"][j]["mapValue"]["fields"]["every"]["integerValue"];
                if (time == sendTime && status == true && days == day)
                {
                    duration5 = schedule["schedule"][j]["mapValue"]["fields"]["duration"]["integerValue"];
                    vTaskResume(task5_handle);
                    timmerCheck5 = false;
                }
                else if (time == sendTime && status == true && day > 7)
                {
                    duration5 = schedule["schedule"][j]["mapValue"]["fields"]["duration"]["integerValue"];
                    vTaskResume(task5_handle);
                    timmerCheck5 = false;
                }
            }
        }
    }

    // end schedule=========================================================================

    // action check ========================================================================

    for (int i = 0; i < actions["actions"].size(); i++)
    {
        String output = actions["actions"][i]["mapValue"]["fields"]["output"]["stringValue"];
        if (output == "out1")
        {
            String con = actions["actions"][i]["mapValue"]["fields"]["if"]["stringValue"];
            bool status = actions["actions"][i]["mapValue"]["fields"]["status"]["booleanValue"];
            if (con == "temp" && status == true)
            {
                int value = actions["actions"][i]["mapValue"]["fields"]["value"]["integerValue"];
                String _con = actions["actions"][i]["mapValue"]["fields"]["con"]["stringValue"];
                bool state = actions["actions"][i]["mapValue"]["fields"]["state"]["booleanValue"];
                if (_con == ">=")
                {
                    if (value >= t)
                    {
                        digitalWrite(out1, state);
                    }
                }
                else if (_con == "<=")
                {
                    if (value <= t)
                    {
                        digitalWrite(out1, state);
                    }
                }
            }
            else if (con == "humi" && status == true)
            {
                int value = actions["actions"][i]["mapValue"]["fields"]["value"]["integerValue"];
                String _con = actions["actions"][i]["mapValue"]["fields"]["con"]["stringValue"];
                bool state = actions["actions"][i]["mapValue"]["fields"]["state"]["booleanValue"];
                if (_con == ">=")
                {
                    if (value >= h)
                    {
                        digitalWrite(out1, state);
                    }
                }
                else if (_con == "<=")
                {
                    if (value <= h)
                    {
                        digitalWrite(out1, state);
                    }
                }
            }
            else if (con == "soil" && status == true)
            {
                int value = actions["actions"][i]["mapValue"]["fields"]["value"]["integerValue"];
                String _con = actions["actions"][i]["mapValue"]["fields"]["con"]["stringValue"];
                bool state = actions["actions"][i]["mapValue"]["fields"]["state"]["booleanValue"];
                if (_con == ">=")
                {
                    if (value >= kelembabanTanah)
                    {
                        digitalWrite(out1, state);
                    }
                }
                else if (_con == "<=")
                {
                    if (value <= kelembabanTanah)
                    {
                        digitalWrite(out1, state);
                    }
                }
            }
            else if (con == "ph" && status == true)
            {
                int value = actions["actions"][i]["mapValue"]["fields"]["value"]["integerValue"];
                String _con = actions["actions"][i]["mapValue"]["fields"]["con"]["stringValue"];
                bool state = actions["actions"][i]["mapValue"]["fields"]["state"]["booleanValue"];
                if (_con == ">=")
                {
                    if (value >= phTanahValue)
                    {
                        digitalWrite(out1, state);
                    }
                }
                else if (_con == "<=")
                {
                    if (value <= phTanahValue)
                    {
                        digitalWrite(out1, state);
                    }
                }
            }
        }
        else if (output == "out2")
        {
            String con = actions["actions"][i]["mapValue"]["fields"]["if"]["stringValue"];
            bool status = actions["actions"][i]["mapValue"]["fields"]["status"]["booleanValue"];
            if (con == "temp" && status == true)
            {
                int value = actions["actions"][i]["mapValue"]["fields"]["value"]["integerValue"];
                String _con = actions["actions"][i]["mapValue"]["fields"]["con"]["stringValue"];
                bool state = actions["actions"][i]["mapValue"]["fields"]["state"]["booleanValue"];
                if (_con == ">=")
                {
                    if (value >= t)
                    {
                        digitalWrite(out2, state);
                    }
                }
                else if (_con == "<=")
                {
                    if (value <= t)
                    {
                        digitalWrite(out2, state);
                    }
                }
            }
            else if (con == "humi" && status == true)
            {
                int value = actions["actions"][i]["mapValue"]["fields"]["value"]["integerValue"];
                String _con = actions["actions"][i]["mapValue"]["fields"]["con"]["stringValue"];
                bool state = actions["actions"][i]["mapValue"]["fields"]["state"]["booleanValue"];
                if (_con == ">=")
                {
                    if (value >= t)
                    {
                        digitalWrite(out2, state);
                    }
                }
                else if (_con == "<=")
                {
                    if (value <= t)
                    {
                        digitalWrite(out2, state);
                    }
                }
            }
            else if (con == "soil" && status == true)
            {
                int value = actions["actions"][i]["mapValue"]["fields"]["value"]["integerValue"];
                String _con = actions["actions"][i]["mapValue"]["fields"]["con"]["stringValue"];
                bool state = actions["actions"][i]["mapValue"]["fields"]["state"]["booleanValue"];
                if (_con == ">=")
                {
                    if (value >= t)
                    {
                        digitalWrite(out2, state);
                    }
                }
                else if (_con == "<=")
                {
                    if (value <= t)
                    {
                        digitalWrite(out2, state);
                    }
                }
            }
            else if (con == "ph" && status == true)
            {
                int value = actions["actions"][i]["mapValue"]["fields"]["value"]["integerValue"];
                String _con = actions["actions"][i]["mapValue"]["fields"]["con"]["stringValue"];
                bool state = actions["actions"][i]["mapValue"]["fields"]["state"]["booleanValue"];
                if (_con == ">=")
                {
                    if (value >= t)
                    {
                        digitalWrite(out2, state);
                    }
                }
                else if (_con == "<=")
                {
                    if (value <= t)
                    {
                        digitalWrite(out2, state);
                    }
                }
            }
        }
        else if (output == "out3")
        {
            bool status = actions["actions"][i]["mapValue"]["fields"]["status"]["booleanValue"];
            String con = actions["actions"][i]["mapValue"]["fields"]["if"]["stringValue"];
            if (con == "temp" && status == true)
            {
                int value = actions["actions"][i]["mapValue"]["fields"]["value"]["integerValue"];
                String _con = actions["actions"][i]["mapValue"]["fields"]["con"]["stringValue"];
                bool state = actions["actions"][i]["mapValue"]["fields"]["state"]["booleanValue"];
                if (_con == ">=")
                {
                    if (value >= t)
                    {
                        digitalWrite(out3, state);
                    }
                }
                else if (_con == "<=")
                {
                    if (value <= t)
                    {
                        digitalWrite(out3, state);
                    }
                }
            }
            else if (con == "humi" && status == true)
            {
                int value = actions["actions"][i]["mapValue"]["fields"]["value"]["integerValue"];
                String _con = actions["actions"][i]["mapValue"]["fields"]["con"]["stringValue"];
                bool state = actions["actions"][i]["mapValue"]["fields"]["state"]["booleanValue"];
                if (_con == ">=")
                {
                    if (value >= t)
                    {
                        digitalWrite(out3, state);
                    }
                }
                else if (_con == "<=")
                {
                    if (value <= t)
                    {
                        digitalWrite(out3, state);
                    }
                }
            }
            else if (con == "soil" && status == true)
            {
                int value = actions["actions"][i]["mapValue"]["fields"]["value"]["integerValue"];
                String _con = actions["actions"][i]["mapValue"]["fields"]["con"]["stringValue"];
                bool state = actions["actions"][i]["mapValue"]["fields"]["state"]["booleanValue"];
                if (_con == ">=")
                {
                    if (value >= t)
                    {
                        digitalWrite(out3, state);
                    }
                }
                else if (_con == "<=")
                {
                    if (value <= t)
                    {
                        digitalWrite(out3, state);
                    }
                }
            }
            else if (con == "ph" && status == true)
            {
                int value = actions["actions"][i]["mapValue"]["fields"]["value"]["integerValue"];
                String _con = actions["actions"][i]["mapValue"]["fields"]["con"]["stringValue"];
                bool state = actions["actions"][i]["mapValue"]["fields"]["state"]["booleanValue"];
                if (_con == ">=")
                {
                    if (value >= t)
                    {
                        digitalWrite(out3, state);
                    }
                }
                else if (_con == "<=")
                {
                    if (value <= t)
                    {
                        digitalWrite(out3, state);
                    }
                }
            }
        }
        else if (output == "out4")
        {
            bool status = actions["actions"][i]["mapValue"]["fields"]["status"]["booleanValue"];
            String con = actions["actions"][i]["mapValue"]["fields"]["if"]["stringValue"];
            if (con == "temp" && status == true)
            {
                int value = actions["actions"][i]["mapValue"]["fields"]["value"]["integerValue"];
                String _con = actions["actions"][i]["mapValue"]["fields"]["con"]["stringValue"];
                bool state = actions["actions"][i]["mapValue"]["fields"]["state"]["booleanValue"];
                if (_con == ">=")
                {
                    if (value >= t)
                    {
                        digitalWrite(out4, state);
                    }
                }
                else if (_con == "<=")
                {
                    if (value <= t)
                    {
                        digitalWrite(out4, state);
                    }
                }
            }
            else if (con == "humi" && status == true)
            {
                int value = actions["actions"][i]["mapValue"]["fields"]["value"]["integerValue"];
                String _con = actions["actions"][i]["mapValue"]["fields"]["con"]["stringValue"];
                bool state = actions["actions"][i]["mapValue"]["fields"]["state"]["booleanValue"];
                if (_con == ">=")
                {
                    if (value >= t)
                    {
                        digitalWrite(out4, state);
                    }
                }
                else if (_con == "<=")
                {
                    if (value <= t)
                    {
                        digitalWrite(out4, state);
                    }
                }
            }
            else if (con == "soil" && status == true)
            {
                int value = actions["actions"][i]["mapValue"]["fields"]["value"]["integerValue"];
                String _con = actions["actions"][i]["mapValue"]["fields"]["con"]["stringValue"];
                bool state = actions["actions"][i]["mapValue"]["fields"]["state"]["booleanValue"];
                if (_con == ">=")
                {
                    if (value >= t)
                    {
                        digitalWrite(out4, state);
                    }
                }
                else if (_con == "<=")
                {
                    if (value <= t)
                    {
                        digitalWrite(out4, state);
                    }
                }
            }
            else if (con == "ph" && status == true)
            {
                int value = actions["actions"][i]["mapValue"]["fields"]["value"]["integerValue"];
                String _con = actions["actions"][i]["mapValue"]["fields"]["con"]["stringValue"];
                bool state = actions["actions"][i]["mapValue"]["fields"]["state"]["booleanValue"];
                if (_con == ">=")
                {
                    if (value >= t)
                    {
                        digitalWrite(out4, state);
                    }
                }
                else if (_con == "<=")
                {
                    if (value <= t)
                    {
                        digitalWrite(out4, state);
                    }
                }
            }
        }
        else if (output == "out5")
        {
            bool status = actions["actions"][i]["mapValue"]["fields"]["status"]["booleanValue"];
            String con = actions["actions"][i]["mapValue"]["fields"]["if"]["stringValue"];
            if (con == "temp" && status == true)
            {
                int value = actions["actions"][i]["mapValue"]["fields"]["value"]["integerValue"];
                String _con = actions["actions"][i]["mapValue"]["fields"]["con"]["stringValue"];
                bool state = actions["actions"][i]["mapValue"]["fields"]["state"]["booleanValue"];
                if (_con == ">=")
                {
                    if (value >= t)
                    {
                        digitalWrite(out5, state);
                    }
                }
                else if (_con == "<=")
                {
                    if (value <= t)
                    {
                        digitalWrite(out5, state);
                    }
                }
            }
            else if (con == "humi" && status == true)
            {
                int value = actions["actions"][i]["mapValue"]["fields"]["value"]["integerValue"];
                String _con = actions["actions"][i]["mapValue"]["fields"]["con"]["stringValue"];
                bool state = actions["actions"][i]["mapValue"]["fields"]["state"]["booleanValue"];
                if (_con == ">=")
                {
                    if (value >= t)
                    {
                        digitalWrite(out5, state);
                    }
                }
                else if (_con == "<=")
                {
                    if (value <= t)
                    {
                        digitalWrite(out5, state);
                    }
                }
            }
            else if (con == "soil" && status == true)
            {
                int value = actions["actions"][i]["mapValue"]["fields"]["value"]["integerValue"];
                String _con = actions["actions"][i]["mapValue"]["fields"]["con"]["stringValue"];
                bool state = actions["actions"][i]["mapValue"]["fields"]["state"]["booleanValue"];
                if (_con == ">=")
                {
                    if (value >= t)
                    {
                        digitalWrite(out5, state);
                    }
                }
                else if (_con == "<=")
                {
                    if (value <= t)
                    {
                        digitalWrite(out5, state);
                    }
                }
            }
            else if (con == "ph" && status == true)
            {
                int value = actions["actions"][i]["mapValue"]["fields"]["value"]["integerValue"];
                String _con = actions["actions"][i]["mapValue"]["fields"]["con"]["stringValue"];
                bool state = actions["actions"][i]["mapValue"]["fields"]["state"]["booleanValue"];
                if (_con == ">=")
                {
                    if (value >= t)
                    {
                        digitalWrite(out5, state);
                    }
                }
                else if (_con == "<=")
                {
                    if (value <= t)
                    {
                        digitalWrite(out5, state);
                    }
                }
            }
        }
    }

    // end action ========================================================================
}

void rainSensor()
{
    if (!digitalRead(pinSensorHujan) && hujan)
    {
        client.publish(String("/" + MACADD + "/data/weather"), "true", false);
        hujan = false;
    }

    if (digitalRead(pinSensorHujan) && !hujan)
    {
        client.publish(String("/" + MACADD + "/data/weather"), "false", false);
        hujan = true;
    }
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

void loop()
{
    client.loop();

    millisTime = millis();

    btn.tick();

    hour = String(timeClient.getHours());
    minutes = String(timeClient.getMinutes());

    if (hour.length() == 1)
    {
        hour = String("0" + hour);
    }

    if (minutes.length() == 1)
    {
        minutes = String("0" + minutes);
    }

    sendTime = String(hour + ":" + minutes);

    // sendTime = String(String(timeClient.getHours()) + ":" + String(timeClient.getMinutes()));
    days = timeClient.getDay();

    // Serial.println(sendTime);
    // Serial.print("convert time = ");
    // Serial.println(sendTime);

    // delay(1000); // ================= remove

    sensorRead();
    // rainSensor();
    sensorDisplay();

    scheduleAndAction();

    if (millisTime - rtdbTime > 5000)
    {
        Firebase.RTDB.setInt(&fbdo, "/" + MACADD + "/data/temp", t);
        Firebase.RTDB.setInt(&fbdo, "/" + MACADD + "/data/humi", h);
        Firebase.RTDB.setInt(&fbdo, "/" + MACADD + "/data/soil", kelembabanTanah);
        Firebase.RTDB.setInt(&fbdo, "/" + MACADD + "/data/ph", phTanahValue);

        rtdbTime = millisTime;
    }

    if (millisTime - sensorTime > 300000)
    {

        json.set("/temp", t);
        json.set("/hum", h);
        json.set("/soil", kelembabanTanah);
        json.set("/ph", phTanahValue);
        json.set("/time", sendTime);

        Firebase.RTDB.pushJSON(&fbdo, "/" + MACADD + "/Sensor", &json);

        sensorTime = millisTime;
    }

    delayMicroseconds(5);
}