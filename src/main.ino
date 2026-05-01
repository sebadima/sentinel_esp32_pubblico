#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include "time.h"
#include "RTClib.h"
#include "Adafruit_FRAM_I2C.h"
#include <esp_task_wdt.h>
#include <DHT.h>

const int FW_VERSION = 270;

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

// DEFINIZIONI PER IL FIX DUPLICATI E CHECKSUM
#define FRAM_WRITE_PTR_LOC 0
#define FRAM_READ_PTR_LOC  2
#define FRAM_DATA_START    4

// CONFIGURAZIONE DHT11
#define DHTPIN 15
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// --- CONFIGURAZIONE RETE ---
#include "secrets.h"

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
  uint16_t checksum;
};

RTC_DS3231 rtc;
Adafruit_FRAM_I2C fram;

uint16_t framWriteAddr = FRAM_DATA_START;
uint16_t framReadAddr  = FRAM_DATA_START;

unsigned long lastMillis = 0;
unsigned long lastSyncMillis = 0;
const long interval = 15000;
const long syncInterval = 30000;
unsigned long lastWiFiRetryMillis = 0;
const unsigned long wifiRetryInterval = 30000;
const unsigned long FORCED_SYNC_INTERVAL = 43200000UL;
int lastDriftSeconds = 0;
#define FRAM_SIZE       32768



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

uint16_t calculateChecksum(Payload p) {
  p.checksum = 0;
  uint16_t sum1 = 0;
  uint16_t sum2 = 0;
  const uint8_t* data = (const uint8_t*)&p;
  for (size_t i = 0; i < sizeof(Payload); ++i) {
    sum1 = (sum1 + data[i]) % 255;
    sum2 = (sum2 + sum1) % 255;
  }
  return (sum2 << 8) | sum1;
}

void syncTime(bool force = false) {
  if (WiFi.status() != WL_CONNECTED) return;
  Wire.beginTransmission(0x68);
  Wire.write(0x0F);
  if (Wire.endTransmission() != 0) return;
  Wire.requestFrom(0x68, 1);
  byte status = Wire.read();
  bool oscillatorStopped = (status & 0b10000000);

  static unsigned long lastPeriodicSync = 0;
  bool needPeriodicSync = (millis() - lastPeriodicSync >= FORCED_SYNC_INTERVAL);
  bool needSync = force || oscillatorStopped || needPeriodicSync;
  if (!needSync) return;
  if (needPeriodicSync) lastPeriodicSync = millis();

  Serial.println("[NTP] FETCHING_ATOMIC_TIME...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    time_t ntpTime = mktime(&timeinfo);
    time_t rtcTime = rtc.now().unixtime();
    lastDriftSeconds = abs((long)(ntpTime - rtcTime));
    if (oscillatorStopped) lastDriftSeconds = -1;
    rtc.adjust(DateTime(ntpTime));
    Wire.beginTransmission(0x68);
    Wire.write(0x0F);
    Wire.write(status & 0b01111111);
    Wire.endTransmission();
    Serial.printf("[RTC_SYNC] Drift CORRETTO: %d sec | Prossimo sync tra 12h\n", lastDriftSeconds);
  } else {
    Serial.println("[SYSTEM][ERR] NTP_SYNC_FAILED");
    if (needPeriodicSync) lastPeriodicSync = 0;
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
  p.checksum = calculateChecksum(p);

  // 1. CONTROLLO PREVENTIVO GIRO DI BOA
  // Se il record NON ci sta fisicamente alla fine della memoria,
  // ricominciamo subito dall'inizio per non sfasciare i puntatori a indirizzo 0.
  if (framWriteAddr + sizeof(Payload) > FRAM_SIZE) {
    framWriteAddr = FRAM_DATA_START;
  }

  uint16_t nextWriteAddr = framWriteAddr + sizeof(Payload);
  if (nextWriteAddr > FRAM_SIZE) { // Ulteriore sicurezza
    nextWriteAddr = FRAM_DATA_START;
  }

  // 2. PROTEZIONE OVERFLOW (Se la scrittura raggiunge la lettura)
  if (nextWriteAddr == framReadAddr) {
    framReadAddr += sizeof(Payload);
    // Anche per la lettura, se non c'è spazio per il prossimo record, torna all'inizio
    if (framReadAddr + sizeof(Payload) > FRAM_SIZE) {
      framReadAddr = FRAM_DATA_START;
    }
    fram.write(FRAM_READ_PTR_LOC, (uint8_t*)&framReadAddr, 2);
    Serial.println("[FRAM][WARN] Memoria piena! Scarto il record più vecchio.");
  }

  // 3. SCRITTURA FISICA
  fram.write(framWriteAddr, (uint8_t*)&p, sizeof(Payload));

  uint16_t savedAddr = framWriteAddr;
  framWriteAddr = nextWriteAddr;

  // 4. PERSISTENZA DEL PUNTATORE DI SCRITTURA
  fram.write(FRAM_WRITE_PTR_LOC, (uint8_t*)&framWriteAddr, 2);

  Serial.printf("[FRAM][STORE] W_ADDR: %d | R_ADDR: %d | TS: %s | CHK:0x%04X\n",
                savedAddr, framReadAddr, p.timestamp, p.checksum);
}

int sendToFlask(Payload p, int rtcStatus, int drift) {
  WiFiClient client;
  HTTPClient http;
  http.setConnectTimeout(3000);
  http.begin(client, serverUrl);
  http.addHeader("Content-Type", "application/json");
  StaticJsonDocument<256> doc;
  doc["version"] = FW_VERSION;
  doc["status"] = rtcStatus;
  doc["drift"] = drift;
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
  dht.begin();
  if (!rtc.begin()) { Serial.println("[BOOT][FATAL] RTC_NOT_FOUND"); while(1); }
  if (!fram.begin(0x50)) { Serial.println("[BOOT][FATAL] FRAM_NOT_FOUND"); while(1); }

  // === RECOVERY AVANZATO CON CHECKSUM + GESTIONE WRAP ===
  fram.read(FRAM_WRITE_PTR_LOC, (uint8_t*)&framWriteAddr, 2);
  fram.read(FRAM_READ_PTR_LOC, (uint8_t*)&framReadAddr, 2);

  if (framWriteAddr < FRAM_DATA_START || framWriteAddr >= FRAM_SIZE) framWriteAddr = FRAM_DATA_START;
  if (framReadAddr  < FRAM_DATA_START || framReadAddr  >= FRAM_SIZE) framReadAddr  = FRAM_DATA_START;

  // Scansione per trovare l'ultimo record valido (gestisce anche il wrap)
  uint16_t addr = framWriteAddr;
  bool wrapped = false;

  while (true) {
    if (addr + sizeof(Payload) > FRAM_SIZE) {
      if (wrapped) break;               // già completato un giro → fine
      addr = FRAM_DATA_START;
      wrapped = true;
      continue;
    }

    Payload temp;
    fram.read(addr, (uint8_t*)&temp, sizeof(Payload));

    if (temp.checksum != 0 && temp.checksum == calculateChecksum(temp)) {
      addr += sizeof(Payload);
      framWriteAddr = addr;
    } else {
      break;
    }
  }

  // Persisti il puntatore recuperato
  fram.write(FRAM_WRITE_PTR_LOC, (uint8_t*)&framWriteAddr, 2);

  // Allineamento sicuro del readAddr (anche dopo wrap)
  if (framReadAddr == framWriteAddr) {
    // coda vuota o piena - va bene così
  } else if ((framReadAddr < framWriteAddr && !wrapped) ||
             (framReadAddr > framWriteAddr && wrapped)) {
    // configurazione normale
  } else {
    // read è rimasto indietro dopo un wrap → recuperiamo
    framReadAddr = framWriteAddr;
  }

  Serial.printf("[FRAM][BOOT] Write: %d | Read: %d\n", framWriteAddr, framReadAddr);

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
    triggerOTA();
  } else {
    Serial.println("\n[BOOT] WIFI_OFFLINE | Proceeding with RTC only");
  }
  syncTime();
  Serial.println("[BOOT] --- SENTINEL - READY ---\n");
}

void loop() {
  esp_task_wdt_reset();

  // 1. GESTIONE RICONNESSIONE WIFI
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWiFiRetryMillis >= wifiRetryInterval) {
      lastWiFiRetryMillis = millis();
      Serial.println("[WIFI] Connection lost. Attempting to reconnect...");
      WiFi.disconnect();
      WiFi.begin(ssid, password);
    }
  }

  // 2. SINCRONIZZAZIONE PERIODICA (RTC e OTA)
  if (millis() - lastSyncMillis >= syncInterval) {
    lastSyncMillis = millis();
    syncTime();
    triggerOTA();
  }

  // 3. CAMPIONAMENTO E INVIO DATI (Ogni 'interval')
  if (millis() - lastMillis >= interval) {
    lastMillis = millis();

    DateTime now = rtc.now();
    Payload current;

    // AZZERAMENTO MEMORIA STRUCT PER CHECKSUM AFFIDABILE
    memset(&current, 0, sizeof(Payload));

    snprintf(current.timestamp, sizeof(current.timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());

    current.t1 = readADC(0x4283);
    float dht_temp = dht.readTemperature();
    current.t2 = isnan(dht_temp) ? -99.0 : dht_temp;
    current.h1 = 0.0;
    current.h2 = 0.0;
    current.t_box = (temprature_sens_read() - 32) / 1.8;

    int currentStatus = 0;
    if (lastDriftSeconds == -1) currentStatus = 1;
    else if (lastDriftSeconds > 10) currentStatus = 2;

    Serial.printf("[LOCAL] %s | DRIFT:%d | VIS:%.3fV DHT:%.1fC CORE:%.1fC\n",
                  current.timestamp, lastDriftSeconds, current.t1, current.t2, current.t_box);

    bool currentSent = false;

    if (WiFi.status() == WL_CONNECTED) {

      // === SVUOTAMENTO BUFFER FRAM (FLUSH) ===
      while (framReadAddr != framWriteAddr) {
        esp_task_wdt_reset();

        // CONTROLLO PREVENTIVO GIRO DI BOA LETTURA
        // Deve essere identico a quello in saveToFram()
        if (framReadAddr + sizeof(Payload) > FRAM_SIZE) {
          framReadAddr = FRAM_DATA_START;
        }

        Payload old;
        fram.read(framReadAddr, (uint8_t*)&old, sizeof(Payload));

        if (sendToFlask(old, 0, 0) == 201) {
          // Se invio OK, avanza il puntatore di lettura
          framReadAddr += sizeof(Payload);

          // Verifica se l'avanzamento ha raggiunto la fine
          if (framReadAddr + sizeof(Payload) > FRAM_SIZE) {
            framReadAddr = FRAM_DATA_START;
          }

          // Salva permanentemente il nuovo puntatore di lettura
          fram.write(FRAM_READ_PTR_LOC, (uint8_t*)&framReadAddr, 2);

          yield();
        } else {
          Serial.println("[FRAM][SYNC][ERR] Invio fallito, interruzione.");
          break;
        }
      }

      // === INVIO DEL DATO CORRENTE IN TEMPO REALE ===
      if (sendToFlask(current, currentStatus, lastDriftSeconds) == 201) {
        currentSent = true;
      }
    }

    // === SALVATAGGIO IN FRAM (Se non inviato o se offline) ===
    if (!currentSent) {
      saveToFram(current);
    }
  }
}
