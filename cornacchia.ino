#include <WiFiManager.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <esp32FOTA.hpp>

const int BUTTON = 21;

char botToken[48],
  chatID[12],
  version[] = "1.15",
  fotaUrl[128] = "http://";
WiFiManager wifiManager(Serial);
WiFiClientSecure secured_client;
UniversalTelegramBot bot(botToken, secured_client);
esp32FOTA esp32FOTA("CORNACCHIA", version);

void saveToEEPROM() {
  EEPROM.begin(sizeof botToken - 1 + sizeof chatID - 1 + sizeof fotaUrl - 1);
  EEPROM.writeBytes(0, botToken, sizeof botToken - 1);
  int n = sizeof botToken - 1;
  EEPROM.writeBytes(n, chatID, sizeof chatID - 1);
  n += sizeof chatID - 1;
  EEPROM.writeBytes(n, fotaUrl, sizeof fotaUrl - 1);
  EEPROM.commit();
}

void loadFromEEPROM() {
  EEPROM.begin(sizeof botToken - 1 + sizeof chatID - 1 + sizeof fotaUrl - 1);
  if (0xFF == EEPROM.readByte(0)) return;
  Serial.println("Leggo dati memorizzati");
  EEPROM.readBytes(0, botToken, sizeof botToken - 1);
  int n = sizeof botToken - 1;
  EEPROM.readBytes(n, chatID, sizeof chatID - 1);
  n += sizeof chatID - 1;
  if (0xFF == EEPROM.readByte(n)) return;
  EEPROM.readBytes(n, fotaUrl, sizeof fotaUrl - 1);
}

void setup() {
  int n = 0;

  Serial.begin(115200);
  Serial.println("VIA");

  pinMode(BUTTON, INPUT_PULLUP);
  if (digitalRead(BUTTON) == LOW) {
    Serial.println("Cancelazione EEPROM");
    wifiManager.resetSettings();
  } 
  loadFromEEPROM();

  esp32FOTA.setManifestURL(fotaUrl);
  esp32FOTA.setProgressCb([](size_t progress, size_t size) {
    Serial.printf("aggiornamento: %d%%\n",(100UL*progress)/size);
    if (progress == 0) {
      bot.sendMessage(chatID, "inizio aggiornamento");
    } else if (progress == size) {
      bot.sendMessage(chatID, "fine aggiornamento");
    }
  });

  wifiManager.setConnectTimeout(120);
  wifiManager.setWiFiAutoReconnect(true);
  static WiFiManagerParameter tokenParam("token", "bot token", botToken, sizeof botToken - 1),
    chatParam("chat", "chat ID", chatID, sizeof chatID - 1),
    fotaParam("fota", "FOTA URL", fotaUrl, sizeof fotaUrl - 1);
  wifiManager.addParameter(&tokenParam);
  wifiManager.addParameter(&chatParam);
  wifiManager.addParameter(&fotaParam);

  wifiManager.setSaveConfigCallback([]() {
    strcpy(botToken, tokenParam.getValue());
    strcpy(chatID, chatParam.getValue());
    strcpy(fotaUrl, fotaParam.getValue());
    Serial.printf("salvo su EEPROM TOKEN: %s, CHAT ID: %s, FOTA: %s\n", botToken, chatID, fotaUrl);
    saveToEEPROM();
  });

  while (!wifiManager.autoConnect("cornacchia")) {
    Serial.println("Provo a connettermi");
    delay(3000);
    if (++n == 10) ESP.restart();
  }

  Serial.printf("TOKEN: %s, CHAT ID: %s, FOTA: %s\n", botToken, chatID, fotaUrl);
  bot.updateToken(botToken);

  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);  // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
  bot.sendMessage(chatID, (String("Avviato cornacchia ") + version + ", IP:  " + WiFi.localIP().toString()).c_str(), "");
}

void loop() {
  static uint64_t tLastFotaCheck = 0, tLastMessage = 0;

  if (digitalRead(BUTTON) == LOW) {
    Serial.println("cornacchia!");
    if (tLastMessage == 0 || millis() - tLastMessage > 60 * 1000) {
      Serial.println("invio messaggio");
      if (bot.sendMessage(chatID, "cornacchia!", ""))
        tLastMessage = millis();
      else
        Serial.println("invio fallito");
    }
    delay(200);
  }
  else
    delay(100);

  if (tLastFotaCheck == 0 || millis() - tLastFotaCheck > 60 * 1000) {
    Serial.printf("controlo aggiornamenti su %s\n", fotaUrl);
    esp32FOTA.handle();
    tLastFotaCheck = millis();
  }
}
