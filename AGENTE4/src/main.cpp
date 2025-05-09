#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <FirebaseJson.h>
#include <time.h>

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
bool firebaseInitialized = false;

void setupFirebase() {
  if (firebaseInitialized) {
    return; // Evita inicializar Firebase más de una vez
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando a Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println(" conectado.");

  // Configurar NTP
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Sincronizando tiempo...");
  while (time(nullptr) < 100000) {
    Serial.print(".");
    delay(300);
  }
  Serial.println(" tiempo sincronizado.");

  ssl_client.setInsecure();
  Serial.println("Inicializando Firebase...");
  
  // Callback para inicializar Firebase
  initializeApp(aClient, app, getAuth(user_auth), [](AsyncResult &r) {
    if (!r.error().message().isEmpty()) {
      Serial.print("Error al inicializar Firebase: ");
      Serial.println(r.error().message());
    } else {
      Serial.println("Firebase inicializado correctamente.");
      firebaseInitialized = true; // Marca Firebase como inicializado solo si no hay errores
    }
  }, "auth");

  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);
}

// --- Tarea para enviar datos del Agente 1 ---
void TaskAgent1(void *pvParameters) {
  while (true) {
    if (firebaseInitialized) {
      Serial.println("TaskAgent1: Enviando datos a Firebase...");
      FirebaseJson json;
      unsigned long ts = time(nullptr);
      json.set("soc", 73.0);
      json.set("barra", 5.01);
      json.set("iout", 1.25);
      json.set("celda_1", 3.80);
      json.set("celda_2", 3.81);
      json.set("celda_3", 3.79);
      json.set("celda_4", 3.82);
      String jsonStr;
      json.toString(jsonStr, true);
      Database.set(aClient, "Agents/1", jsonStr, [](AsyncResult &r) {
        if (!r.error().message().isEmpty()) {
          Serial.print("TaskAgent1: Error al enviar datos: ");
          Serial.println(r.error().message());
        } else {
          Serial.println("TaskAgent1: Datos enviados correctamente.");
        }
      }, "agent1");
    } else {
      Serial.println("TaskAgent1: Firebase no inicializado.");
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS); // Actualiza cada 1 segundo
  }
}

// --- Tarea para enviar datos del Agente 2 ---
void TaskAgent2(void *pvParameters) {
  while (true) {
    if (firebaseInitialized) {
      Serial.println("TaskAgent2: Enviando datos a Firebase...");
      FirebaseJson json;
      unsigned long ts = time(nullptr);
      json.set("soc", 56.0);
      json.set("barra", 4.96);
      json.set("iout", 0.95);
      json.set("celda_1", 3.70);
      json.set("celda_2", 3.69);
      json.set("celda_3", 3.72);
      json.set("celda_4", 3.68);
      String jsonStr;
      json.toString(jsonStr, true);
      Database.set(aClient, "Agents/2", jsonStr, [](AsyncResult &r) {
        if (!r.error().message().isEmpty()) {
          Serial.print("TaskAgent2: Error al enviar datos: ");
          Serial.println(r.error().message());
        } else {
          Serial.println("TaskAgent2: Datos enviados correctamente.");
        }
      }, "agent2");
    } else {
      Serial.println("TaskAgent2: Firebase no inicializado.");
    }
    vTaskDelay(3000 / portTICK_PERIOD_MS); // Actualiza cada 3 segundos
  }
}

// --- Tarea para enviar datos del Agente 3 ---
void TaskAgent3(void *pvParameters) {
  while (true) {
    if (firebaseInitialized) {
      Serial.println("TaskAgent3: Enviando datos a Firebase...");
      FirebaseJson json;
      unsigned long ts = time(nullptr);
      json.set("soc", 89.0);
      json.set("barra", 5.03);
      json.set("iout", 1.45);
      json.set("celda_1", 3.90);
      json.set("celda_2", 3.91);
      json.set("celda_3", 3.88);
      json.set("celda_4", 3.92);
      String jsonStr;
      json.toString(jsonStr, true);
      Database.set(aClient, "Agents/3", jsonStr, [](AsyncResult &r) {
        if (!r.error().message().isEmpty()) {
          Serial.print("TaskAgent3: Error al enviar datos: ");
          Serial.println(r.error().message());
        } else {
          Serial.println("TaskAgent3: Datos enviados correctamente.");
        }
      }, "agent3");
    } else {
      Serial.println("TaskAgent3: Firebase no inicializado.");
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS); // Actualiza cada 5 segundos
  }
}

// --- Setup principal ---
void setup() {
  Serial.begin(115200);
  setupFirebase();
  xTaskCreatePinnedToCore(TaskAgent1, "Agent1Task", 8192, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(TaskAgent2, "Agent2Task", 8192, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(TaskAgent3, "Agent3Task", 8192, NULL, 1, NULL, 1);
}

void loop() {
  // El loop principal queda vacío, ya que las tareas se ejecutan en FreeRTOS
}
