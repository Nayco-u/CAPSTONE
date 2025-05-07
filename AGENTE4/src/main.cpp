#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include "driver/twai.h" // Para CAN
#include <map>

#define WIFI_SSID "Seguel Garces"
#define WIFI_PASSWORD "44699729"

#define Web_API_KEY "AIzaSyAVt_Gn_2jqQYufbcg4GzsJ6Q6MoozBGYU"
#define DATABASE_URL "https://capstone-b36f2-default-rtdb.firebaseio.com/"
#define USER_EMAIL "capstone.potencia.2025@gmail.com"
#define USER_PASS "potencia2025"

// Firebase
UserAuth user_auth(Web_API_KEY, USER_EMAIL, USER_PASS);
FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
RealtimeDatabase Database;

// Variables globales
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 1000; // 1 segundo
String uid, databasePath;
std::map<int, float> agentData; // id → valor
std::map<int, unsigned long> lastSeen; // id → timestamp

SemaphoreHandle_t dataMutex;

// Inicializa TWAI
void initCAN() {
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_5, GPIO_NUM_4, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  twai_driver_install(&g_config, &t_config, &f_config);
  twai_start();
}

// Recibe datos por CAN
void TaskCANReceive(void *pvParameters) {
  while (1) {
    twai_message_t message;
    if (twai_receive(&message, pdMS_TO_TICKS(10)) == ESP_OK) {
      int id = message.identifier;
      float value;
      memcpy(&value, message.data, sizeof(float));

      xSemaphoreTake(dataMutex, portMAX_DELAY);
      agentData[id] = value;
      lastSeen[id] = millis();
      xSemaphoreGive(dataMutex);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// Envia promedio por CAN
void TaskCANSender(void *pvParameters) {
  while (1) {
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    float sum = 0;
    int count = 0;
    for (auto &[id, val] : agentData) {
      if (millis() - lastSeen[id] < 1500) {
        sum += val;
        count++;
      }
    }
    float avg = (count > 0) ? sum / count : 0.0;
    xSemaphoreGive(dataMutex);

    // Enviar por CAN
    twai_message_t tx_msg = {};
    tx_msg.identifier = 100; // ID para el promedio
    tx_msg.data_length_code = sizeof(float);
    memcpy(tx_msg.data, &avg, sizeof(float));
    twai_transmit(&tx_msg, pdMS_TO_TICKS(10));

    vTaskDelay(sendInterval / portTICK_PERIOD_MS);
  }
}

// Envia datos a Firebase
void TaskFirebase(void *pvParameters) {
  while (1) {
    if (app.ready()) {
      xSemaphoreTake(dataMutex, portMAX_DELAY);
      for (auto &[id, val] : agentData) {
        if (millis() - lastSeen[id] < 1500) {
          String path = "Agents/" + String(id);
          Database.set<float>(aClient, path, val, [](AsyncResult &r) {}, "upload");
        }
      }
      xSemaphoreGive(dataMutex);
    }
    vTaskDelay(sendInterval / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(300);
  ssl_client.setInsecure();

  initializeApp(aClient, app, getAuth(user_auth), [](AsyncResult &r) {}, "auth");
  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);

  initCAN();
  dataMutex = xSemaphoreCreateMutex();

  xTaskCreatePinnedToCore(TaskCANReceive, "CAN_RX", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(TaskCANSender, "CAN_TX", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(TaskFirebase, "Firebase", 8192, NULL, 1, NULL, 0);
}

void loop() {
  app.loop(); // Firebase loop
}
