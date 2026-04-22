#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include "time.h"
#include "RTClib.h"
#include "Adafruit_FRAM_I2C.h"
#include <esp_task_wdt.h>
#include <DHT.h> // AGGIUNTO: Libreria DHT

// --- CONFIGURAZIONE VERSIONE ---
const int FW_VERSION = 104;

// --- DIAGNOSTICA INTERNA ESP32 ---
#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif

// --- CONFIGURAZIONE HARDWARE ---
#define ADS_ADDR 0x48
#define WDT_TIMEOUT 30

// CONFIGURAZIONE DHT11 (Sostituisce A1)
#define DHTPIN 15
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// --- CONFIGURAZIONE RETE ---
#include "secrets.h"   // ← Aggiungi questa riga in cima

// --- CONFIGURAZIONE RETE ---
const char* ssid          = WIFI_SSID;
const char* password      = WIFI_PASSWORD;
const char* serverUrl     = SERVER_URL;
const char* updateUrl     = UPDATE_URL;
const char* ntpServer     = NTP_SERVER;


const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 0;

// --- STRUTTURE E ISTANZE ---
struct Payload {
  char timestamp[20];
  float t1, h1, t2, h2;
  float t_box;
};

RTC_DS3231 rtc;
Adafruit_FRAM_I2C fram;

uint16_t framAddr = 0;
unsigned long lastMillis = 0;
unsigned long lastSyncMillis = 0;
const long interval = 15000;
const long syncInterval = 30000;

// --- FUNZIONI ATOMICHE ---

float readADC(uint16_t config) {
  Wire.beginTransmission(ADS_ADDR);
  Wire.write(0x01);
  Wire.write((uint8_t)(config >> 8));
  Wire.write((uint8_t)(config & 0xFF));
  if (Wire.endTransmission() != 0) return -1.0;
  delay(20);
  Wire.beginTransmission(ADS_ADDR);
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.requestFrom(ADS_ADDR, 2);
  if (Wire.available() == 2) {
    int16_t raw = (Wire.read() << 8) | Wire.read();
    return (float)raw * (4.096 / 32768.0);
  }
  return -1.0;
}

void syncTime() {
  if (WiFi.status() != WL_CONNECTED) return;
  Serial.println("[NTP] FETCHING_ATOMIC_TIME...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
    Serial.println("[SYSTEM] RTC_SYNCED_VIA_NTP");
  } else {
    Serial.println("[SYSTEM][ERR] NTP_SYNC_FAILED");
  }
}

void triggerOTA() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  HTTPClient http;

  Serial.println("[OTA] CHECKING_REMOTE_VERSION...");
  http.begin(client, updateUrl);

  int httpCode = http.GET();
  if (httpCode == 200) {
    StaticJsonDocument<256> doc;
    deserializeJson(doc, http.getString());

    int remoteVersion = doc["version"];
    String downloadUrl = doc["url"];

    if (remoteVersion > FW_VERSION) {
      Serial.printf("[OTA] UPGRADING: V%d -> V%d\n", FW_VERSION, remoteVersion);
      esp_task_wdt_reset();
      httpUpdate.rebootOnUpdate(true);
      t_httpUpdate_return ret = httpUpdate.update(client, downloadUrl);

      if (ret == HTTP_UPDATE_FAILED) {
        Serial.printf("[OTA][ERR] %d: %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      }
    } else {
      Serial.println("[OTA] VERSION_MATCH_OR_LOWER. SKIPPING.");
    }
  }
  http.end();
}

void saveToFram(Payload p) {
  if (framAddr + sizeof(Payload) > 32768) {
    Serial.println("[FRAM][WARN] MEMORY_FULL - OVERWRITING FROM ADDR 0");
    framAddr = 0;
  }
  fram.write(framAddr, (uint8_t*)&p, sizeof(Payload));
  framAddr += sizeof(Payload);
  Serial.printf("[FRAM][STORE] ADDR: %d | TS: %s\n", framAddr - sizeof(Payload), p.timestamp);
}

int sendToFlask(Payload p) {
  WiFiClient client;
  HTTPClient http;
  http.setConnectTimeout(3000);

  http.begin(client, serverUrl);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<256> doc;
  doc["version"] = FW_VERSION;
  doc["status"] = 0;
  doc["timestamp"] = p.timestamp;
  doc["t1"] = p.t1;
  doc["h1"] = p.h1;
  doc["t2"] = p.t2;
  doc["h2"] = p.h2;
  doc["t_box"] = p.t_box;

  String json;
  serializeJson(doc, json);

  int code = http.POST(json);

  if (code < 0) {
    Serial.printf("[HTTP][ERR] POST_FAILED | CODE: %d\n", code);
  }

  http.end();
  return code;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.printf("\n\n[BOOT] --- SENTINEL V%d INIT ---\n", FW_VERSION);

  Wire.begin(21, 22);

  // Inizializzazione DHT11
  dht.begin();

  if (!rtc.begin()) { Serial.println("[BOOT][FATAL] RTC_NOT_FOUND"); while(1); }
  if (!fram.begin(0x50)) { Serial.println("[BOOT][FATAL] FRAM_NOT_FOUND"); while(1); }

  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL);

  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
    esp_task_wdt_reset();
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[BOOT] WIFI_OK | IP: %s\n", WiFi.localIP().toString().c_str());
    syncTime();
    triggerOTA();
  }

  Serial.println("[BOOT] --- SENTINEL - READY ---\n");
}

void loop() {
  esp_task_wdt_reset();

  if (millis() - lastSyncMillis >= syncInterval) {
    lastSyncMillis = millis();
    syncTime();
    triggerOTA();
  }

  if (millis() - lastMillis >= interval) {
    lastMillis = millis();

    DateTime now = rtc.now();
    Payload current;
    snprintf(current.timestamp, sizeof(current.timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());

    // Lettura A0 mantenuta
    current.t1 = readADC(0x4283);

    // Lettura A1 soppressa. Iniezione target DHT11 in t2 con fault tolerance
    float dht_temp = dht.readTemperature();
    current.t2 = isnan(dht_temp) ? -99.0 : dht_temp;

    current.h1 = 0.0;
    current.h2 = 0.0;
    current.t_box = (temprature_sens_read() - 32) / 1.8;

    // Log locale aggiornato per riflettere il nuovo layer fisico
    Serial.printf("[LOCAL] %s | VIS:%.3fV DHT:%.1fC CORE:%.1fC\n", current.timestamp, current.t1, current.t2, current.t_box);

    bool currentSent = false;

    if (WiFi.status() == WL_CONNECTED) {
      if (framAddr > 0) {
        uint16_t readPtr = 0;
        while (readPtr < framAddr) {
          Payload old;
          fram.read(readPtr, (uint8_t*)&old, sizeof(Payload));
          if (sendToFlask(old) == 201) {
            readPtr += sizeof(Payload);
          } else { break; }
        }
        if (readPtr == framAddr) framAddr = 0;
      }

      if (sendToFlask(current) == 201) {
        currentSent = true;
      }
    }

    if (!currentSent) {
      saveToFram(current);
    }
  }
}
