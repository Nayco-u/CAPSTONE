#include "firebase_task.h"

// Manejo de respuestas
void processData(AsyncResult &aResult)
{
    // Exits when no result available when calling from the loop.
    if (!aResult.isResult())
        return;

    if (aResult.isEvent())
    {
        // Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.eventLog().message().c_str(), aResult.eventLog().code());
    }

    if (aResult.isDebug())
    {
        // Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
    }

    if (aResult.isError())
    {
        // Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
    }

    if (aResult.available())
    {
        // Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
    }
}

// Inicializa WiFi
void initWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    ssl_client.setInsecure();  // No verifica certificados SSL (necesario en ESP32 si no cargas CA)
    Serial.println("\nWiFi conectado.");
}

void initFirebase() {
  // Configurar cliente SSL antes de todo (muy importante)

  Serial.println("Inicializando Firebase...");

  // Inicializa la app con autenticación
  initializeApp(aClient, app, getAuth(user_auth), processData, "🔐 authTask");

  // Obtiene el objeto de base de datos
  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);  // Asegúrate de que termine con '/'
}

// Tarea de envío
// Tarea de envío (con loop desacoplado)
void Firebase_Update_Task(void *parameter) {
  int agent_number = local_agent_id;
  if (parameter != nullptr) agent_number = *((int*)parameter);

  JsonWriter writer;
  object_t jsonData, objBarra, objCelda, objBattery, objIBarra, objSOC, objTime;

  const TickType_t loopDelay     = pdMS_TO_TICKS(200);    // frecuencia de app.loop()
  const TickType_t sendInterval  = pdMS_TO_TICKS(1000);  // cada cuánto enviar datos

  TickType_t lastWakeTime = xTaskGetTickCount();
  TickType_t lastSendTime = lastWakeTime;

  while (true) {
    app.loop();  // se ejecuta cada 50 ms

    TickType_t now = xTaskGetTickCount();

    // Si ha pasado más de 1 segundo desde el último envío, actualiza Firebase
    if ((now - lastSendTime) >= sendInterval && app.ready()) {

      if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        String id_str = String(agent_number);

        writer.create(objBarra,   "/" + id_str + "_v_barra",   agentes_data[agent_number].v_barra);
        writer.create(objCelda,   "/" + id_str + "_v_celda1",  agentes_data[agent_number].v_cell1);
        writer.create(objBattery, "/" + id_str + "_i_battery", agentes_data[agent_number].i_battery);
        writer.create(objIBarra,  "/" + id_str + "_i_barra",   agentes_data[agent_number].i_converter);
        writer.create(objSOC,     "/" + id_str + "_soc",       agentes_data[agent_number].soc);
        writer.create(objTime,    "/" + id_str + "_time",      millis() / 1000);

        xSemaphoreGive(dataMutex);
      }

      writer.join(jsonData, 6, objBarra, objCelda, objBattery, objIBarra, objSOC, objTime);
      Database.update<object_t>(aClient, "/", jsonData, processData, "RTDB_Update");

      lastSendTime = now;  // actualiza el tiempo del último envío
    }

    vTaskDelayUntil(&lastWakeTime, loopDelay);  // espera 50 ms antes del próximo ciclo
  }
}