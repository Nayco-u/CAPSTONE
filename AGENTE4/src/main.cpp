#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include "driver/twai.h"

// CAN bus configuration
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4

// Network and Firebase credentials
#define WIFI_SSID "naico_wifi"
#define WIFI_PASSWORD "12345678"

#define Web_API_KEY "AIzaSyAVt_Gn_2jqQYufbcg4GzsJ6Q6MoozBGYU"
#define DATABASE_URL "https://capstone-b36f2-default-rtdb.firebaseio.com/"
#define USER_EMAIL "capstone.potencia.2025@gmail.com"
#define USER_PASS "capstone2025"

// Firebase components
UserAuth user_auth(Web_API_KEY, USER_EMAIL, USER_PASS);
FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
RealtimeDatabase Database;

// Timer variables
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 10000; // 10 seconds in milliseconds

// Task handles
TaskHandle_t taskAgent1;
TaskHandle_t taskAgent2;
TaskHandle_t taskAgent3;

// Estructura para datos de cada agente
typedef struct {
  int16_t barra;
  int16_t celda1;
  unsigned long timestamp;
} AgentData;

AgentData agent1 = {0}, agent2 = {0}, agent3 = {0};
SemaphoreHandle_t dataMutex;

// Function prototypes
void processData(AsyncResult &aResult);

// Wi-Fi initialization
void initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println(" connected.");
}

// Tarea para recibir CAN y actualizar variables globales
void CAN_Receive_Task(void *parameter) {
  while (true) {
    twai_message_t message;
    if (twai_receive(&message, pdMS_TO_TICKS(100)) == ESP_OK) {
      if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        unsigned long now = millis() / 1000;
        switch (message.identifier) {
          case 0x201: // Agente1
            agent1.celda1 = message.data[0] | (message.data[1] << 8);
            agent1.barra = message.data[2] | (message.data[3] << 8);
            agent1.timestamp = now;
            break;
          case 0x202: // Agente2
            agent2.celda1 = message.data[0] | (message.data[1] << 8);
            agent2.barra = message.data[2] | (message.data[3] << 8);
            agent2.timestamp = now;
            break;
          case 0x203: // Agente3
            agent3.celda1 = message.data[0] | (message.data[1] << 8);
            agent3.barra = message.data[2] | (message.data[3] << 8);
            agent3.timestamp = now;
            break;
        }
        xSemaphoreGive(dataMutex);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// Tarea para actualizar Firebase cada segundo, solo si hubo actualización reciente por CAN
void Firebase_Update_Task(void *parameter) {
  JsonWriter writer;
  object_t jsonData, objBarra, objCelda, objTimestamp;
  while (true) {
    unsigned long now = millis() / 1000;
    if (app.ready()) {
      if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        // Agente 1
        if (now - agent1.timestamp <= 3 && app.ready()) {
          writer.create(objBarra, "/barra", agent1.barra / 5330.0);
          writer.create(objCelda, "/celda1", agent1.celda1 / 5330.0);
          writer.create(objTimestamp, "/timestamp", agent1.timestamp);
          writer.join(jsonData, 3, objBarra, objCelda, objTimestamp);
          Database.set<object_t>(aClient, "/Agents/Agent1", jsonData, processData, "RTDB_Agent1_JSON");
        }
        // Agente 2
        if (now - agent2.timestamp <= 3 && app.ready()) {
          writer.create(objBarra, "/barra", agent2.barra / 5330.0);
          writer.create(objCelda, "/celda1", agent2.celda1 / 5330.0);
          writer.create(objTimestamp, "/timestamp", agent2.timestamp);
          writer.join(jsonData, 3, objBarra, objCelda, objTimestamp);
          Database.set<object_t>(aClient, "/Agents/Agent2", jsonData, processData, "RTDB_Agent2_JSON");
        }
        // Agente 3
        if (now - agent3.timestamp <= 3 && app.ready()) {
          writer.create(objBarra, "/barra", agent3.barra / 5330.0);
          writer.create(objCelda, "/celda1", agent3.celda1 / 5330.0);
          writer.create(objTimestamp, "/timestamp", agent3.timestamp);
          writer.join(jsonData, 3, objBarra, objCelda, objTimestamp);
          Database.set<object_t>(aClient, "/Agents/Agent3", jsonData, processData, "RTDB_Agent3_JSON");
        }
        xSemaphoreGive(dataMutex);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi
  initWiFi();

  // Configure SSL client
  ssl_client.setInsecure();

  // Initialize Firebase
  initializeApp(aClient, app, getAuth(user_auth), processData, "🔐 authTask");
  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);

  // Inicializar CAN
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  twai_driver_install(&g_config, &t_config, &f_config);
  twai_start();

  dataMutex = xSemaphoreCreateMutex();

  // Create tasks for each agent
  xTaskCreatePinnedToCore(CAN_Receive_Task, "CAN_RX", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(Firebase_Update_Task, "FB_Update", 8192, NULL, 1, NULL, 1);
}

void loop() {
  // Maintain authentication and async tasks
  app.loop();
}

// Callback to handle Firebase responses
void processData(AsyncResult &aResult) {
  if (!aResult.isResult())
    return;

  if (aResult.isEvent())
    Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.eventLog().message().c_str(), aResult.eventLog().code());

  if (aResult.isDebug())
    Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());

  if (aResult.isError())
    Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());

  if (aResult.available())
    Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
}