#include "firebase_task.h"

// Inicializa WiFi
void initWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi conectado.");
}

// Inicializa Firebase
void initFirebase() {
  app.loop();
  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);
}

// Manejo de respuestas
void databaseResult(AsyncResult &aResult) {
  if (!aResult.isResult()) return;

  if (aResult.isEvent())
    Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(),
                    aResult.eventLog().message().c_str(), aResult.eventLog().code());

  if (aResult.isError())
    Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(),
                    aResult.error().message().c_str(), aResult.error().code());

  if (aResult.available())
    Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
}

// Tarea de envío
void Firebase_Update_Task(void *parameter) {
  int agent_number = local_agent_id;
  if (parameter != nullptr) agent_number = *((int*)parameter);

  JsonWriter writer;
  object_t jsonData, objBarra, objCelda, objBattery, objIBarra, objSOC, objTime;

  char path[32];
  snprintf(path, sizeof(path), "/Agents/%d", agent_number);

  while (true) {
    app.loop();

    if (app.ready()) {
      if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        writer.create(objBarra,   "/v_barra",   ads1.computeVolts(agentes_data[local_agent_id].v_barra) * 2);
        writer.create(objCelda,   "/v_celda1",  ads1.computeVolts(agentes_data[local_agent_id].v_cell1));
        writer.create(objBattery, "/i_battery", ads2.computeVolts(agentes_data[local_agent_id].i_battery) / 0.103);
        writer.create(objIBarra,  "/i_barra",   ads2.computeVolts(agentes_data[local_agent_id].i_converter) / 0.103);
        writer.create(objSOC,     "/soc",       agentes_data[local_agent_id].soc / 100.0);
        writer.create(objTime,    "/time",      millis() / 1000);
        xSemaphoreGive(dataMutex);
      }

      writer.join(jsonData, 6, objBarra, objCelda, objBattery, objIBarra, objSOC, objTime);
      Database.set<object_t>(aClient, path, jsonData, databaseResult, "RTDB_Update");

      vTaskDelay(pdMS_TO_TICKS(5000));
    } else {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
}