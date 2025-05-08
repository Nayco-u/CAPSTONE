#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <FirebaseJson.h>

// --- Configuración Wi-Fi y Firebase ---
#define WIFI_SSID "Seguel Garces"
#define WIFI_PASSWORD "44699729"

#define Web_API_KEY "AIzaSyAVt_Gn_2jqQYufbcg4GzsJ6Q6MoozBGYU"
#define DATABASE_URL "https://capstone-b36f2-default-rtdb.firebaseio.com/"
#define USER_EMAIL "capstone.potencia.2025@gmail.com"
#define USER_PASS "potencia2025"

UserAuth user_auth(Web_API_KEY, USER_EMAIL, USER_PASS);
FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
RealtimeDatabase Database;

// --- Setup Firebase ---
void setupFirebase() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando a Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println(" conectado.");

  ssl_client.setInsecure();
  initializeApp(aClient, app, getAuth(user_auth), [](AsyncResult &r) {}, "auth");
  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);
}

// --- Envío simulado de datos ---
void TaskFirebase(void *pvParameters) {
  unsigned long t1 = 0, t2 = 0, t3 = 0;

  while (true) {
    if (app.ready()) {
      unsigned long now = millis();
      unsigned long ts = time(nullptr); // requiere NTP o tiempo confiable

      if (now - t1 >= 500) {
        FirebaseJson json;
        json.set("soc", 73.0);
        json.set("barra", 5.01);
        json.set("iout", 1.25);
        json.set("celda_1", 3.80);
        json.set("celda_2", 3.81);
        json.set("celda_3", 3.79);
        json.set("celda_4", 3.82);
        json.set("timestamp", ts);
        String jsonStr;
        json.toString(jsonStr, true); // Serializa FirebaseJson a String
        Database.set(aClient, "Agents/1", jsonStr, [](AsyncResult &r) {}, "agent1");
        t1 = now;
      }

      if (now - t2 >= 1000) {
        FirebaseJson json;
        json.set("soc", 56.0);
        json.set("barra", 4.96);
        json.set("iout", 0.95);
        json.set("celda_1", 3.70);
        json.set("celda_2", 3.69);
        json.set("celda_3", 3.72);
        json.set("celda_4", 3.68);
        json.set("timestamp", ts);
        String jsonStr;
        json.toString(jsonStr, true); // Serializa FirebaseJson a String
        Database.set(aClient, "Agents/2", jsonStr, [](AsyncResult &r) {}, "agent2");
        t2 = now;
      }

      if (now - t3 >= 2000) {
        FirebaseJson json;
        json.set("soc", 89.0);
        json.set("barra", 5.03);
        json.set("iout", 1.45);
        json.set("celda_1", 3.90);
        json.set("celda_2", 3.91);
        json.set("celda_3", 3.88);
        json.set("celda_4", 3.92);
        json.set("timestamp", ts);
        String jsonStr;
        json.toString(jsonStr, true); // Serializa FirebaseJson a String
        Database.set(aClient, "Agents/3", jsonStr, [](AsyncResult &r) {}, "agent3");
        t3 = now;
      }
    }

    app.loop(); // Mantener conexión con Firebase
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// --- Setup principal ---
void setup() {
  Serial.begin(115200);
  setupFirebase();
  xTaskCreatePinnedToCore(TaskFirebase, "FirebaseTask", 8192, NULL, 1, NULL, 1);
}

void loop() {
  app.loop();
}
