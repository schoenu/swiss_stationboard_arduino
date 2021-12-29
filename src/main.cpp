#include <Arduino.h>

#include <TFT_eSPI.h> 
#include <Button2.h>
#include <WiFi.h> 
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "user_defines.h"

const String start_url = "http://fahrplan.search.ch/api/stationboard.json?stop=";
const String end_url = "&limit=21&show_tracks=1&show_subsequent_stops=0&show_delays=1&show_trackchanges=1&transportation_types=train";

String serverPath = start_url + station_id + end_url;

#define ADC_EN          14
#define ADC_PIN         34
#define BUTTON_1        35
#define BUTTON_2        0

TFT_eSPI tft = TFT_eSPI(135, 240); // Invoke custom library
Button2 btn1(BUTTON_1);
Button2 btn2(BUTTON_2);

char buff[512];
int btnCick = false;
unsigned long tStart;
int start_nr = 0;
DynamicJsonDocument doc(8000);

void updateStationBoard(void);
void getTimetable(void);

void wifi_scan()
{
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);

    tft.drawString("Scan Network", tft.width() / 2, tft.height() / 2);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    int16_t n = WiFi.scanNetworks();
    tft.fillScreen(TFT_BLACK);
    if (n == 0) {
        tft.drawString("no networks found", tft.width() / 2, tft.height() / 2);
    } else {
        tft.setTextDatum(TL_DATUM);
        tft.setCursor(0, 0);
        Serial.printf("Found %d net\n", n);
        for (int i = 0; i < n; ++i) {
            sprintf(buff,
                    "[%d]:%s(%d)",
                    i + 1,
                    WiFi.SSID(i).c_str(),
                    WiFi.RSSI(i));
            tft.println(buff);
        }
    }
    WiFi.mode(WIFI_OFF);
}

//! Long time delay, it is recommended to use shallow sleep, which can effectively reduce the current consumption
void espDelay(int ms)
{   
    esp_sleep_enable_timer_wakeup(ms * 1000);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    esp_light_sleep_start();
}

void button_init()
{
    btn1.setPressedHandler([](Button2 & b) {
        Serial.println("Update Timetable");
        btnCick = true;
    });

    btn2.setPressedHandler([](Button2 & b) {
        Serial.println("Change page");
        start_nr +=5;
        updateStationBoard();
        tStart = millis();
    });
}

void button_loop()
{
    btn1.loop();
    btn2.loop();
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Start");
    tft.init();
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);

    if (TFT_BL > 0) { // TFT_BL has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
         pinMode(TFT_BL, OUTPUT); // Set backlight pin to output mode
         digitalWrite(TFT_BL, TFT_BACKLIGHT_ON); // Turn backlight on. TFT_BACKLIGHT_ON has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
    }
    tft.setRotation(1);

    button_init();
    WiFi.begin(ssid, password);

    Serial.print("Connecting");
    tft.setTextDatum(TL_DATUM);
    tft.setCursor(0, 0);
    tft.println("Connecting");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        tft.print(".");
    }
    tft.println("");
    Serial.println();

    Serial.print("Connected, IP address: ");
    Serial.println(WiFi.localIP());
    tft.println("Connected, IP address: ");
    tft.println(WiFi.localIP());

    Serial.println(serverPath);
    getTimetable();
    updateStationBoard();
}

void printTrain(int number, String line, String terminal_name, String time, String track, String dep_delay){
    String time_short = time.substring(11,16);
    tft.setTextColor(TFT_WHITE);
    if ((line == highlight1_line) && (terminal_name ==highlight1_destination)){
        tft.setTextColor(TFT_YELLOW);
    }
    if ((line == highlight2_line) && (terminal_name ==highlight2_destination)){
        tft.setTextColor(TFT_ORANGE);
    }
    if ((line == highlight3_line) && (terminal_name ==highlight3_destination)){
        tft.setTextColor(TFT_SKYBLUE);
    }
    tft.setCursor(0, number);
    tft.print(line);
    tft.setCursor(40, number);
    tft.print(terminal_name);
    tft.setCursor(180, number);
    tft.print(time_short);
    tft.setCursor(220, number);
    if (dep_delay!="null"){
        tft.print(dep_delay);
    }

    if (dep_delay=="X"){
        tft.drawLine(0, number+3, 240, number+3, TFT_RED);
    }
}

void getTimetable(void){
    HTTPClient http;

    // Send request
    http.useHTTP10(true);
    http.begin(serverPath);
    http.GET();

    // Parse response
    deserializeJson(doc, http.getStream());
    http.end();
}

void updateStationBoard(){
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);
    tft.setCursor(0, 0);
    tft.setTextColor(TFT_WHITE);
    tft.print("Stationboard ");

    // Read values
    tft.println(doc["stop"]["name"].as<String>());
    int j = 0;
    for (int i = (0 + start_nr); i < (start_nr + 11); ++i) {
        printTrain(
        (j*10 + 20),
        doc["connections"][i]["line"].as<String>(),
        doc["connections"][i]["terminal"]["name"].as<String>(),
        doc["connections"][i]["time"].as<String>(),
        doc["connections"][i]["track"].as<String>(),
        doc["connections"][i]["dep_delay"].as<String>()
        );
        j ++;
    }

    tStart = millis();
}

void loop()
{   
    if (millis() < (tStart + 40000)){
        if (btnCick) {
        start_nr = 0;
        getTimetable();
        updateStationBoard();
        tStart = millis();
        btnCick = false;
        }
        button_loop();
    }
    else {
        tft.writecommand(TFT_DISPOFF);
        tft.writecommand(TFT_SLPIN);
        esp_sleep_enable_ext1_wakeup(GPIO_SEL_35, ESP_EXT1_WAKEUP_ALL_LOW);
        esp_deep_sleep_start();
    }
    
}
