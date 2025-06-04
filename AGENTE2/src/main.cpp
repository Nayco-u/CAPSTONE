#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include "driver/ledc.h"
#include "driver/twai.h"

// Pines para CAN
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4

// Network and Firebase credentials
#define WIFI_SSID "Red_Capstone"
#define WIFI_PASSWORD "kkg9-nbfu-297p"

#define Web_API_KEY "AIzaSyAVt_Gn_2jqQYufbcg4GzsJ6Q6MoozBGYU"
#define DATABASE_URL "https://capstone-b36f2-default-rtdb.firebaseio.com/"
#define USER_EMAIL "capstone.potencia.2025@gmail.com"
#define USER_PASS "capstone2025"

// User functions
void asyncCB(AsyncResult &aResult);
void processData(AsyncResult &aResult);
void initWiFi();
void setupPWM();

// Variables globales compartidas
int16_t v_cell1 = 0;       // Voltaje celda 1 en mV
int16_t i_battery = 0;     // Corriente de la batería en mA
int16_t i_converter = 0;   // Corriente del convertidor en mA
int16_t v_barra = 0;       // Voltaje de la barra en mV
int16_t soc_promedio = 0;  // SOC promedio recibido del maestro
SemaphoreHandle_t dataMutex;

// --- Configuración de PWM ---
const int PWM_PIN = 27;
const ledc_channel_t PWM_CHANNEL = LEDC_CHANNEL_0;
const ledc_timer_t PWM_TIMER = LEDC_TIMER_0;
const ledc_timer_bit_t PWM_RESOLUTION = LEDC_TIMER_10_BIT; // 10 bits = 1024 niveles
uint16_t duty = 0;
const uint16_t freq = 20000;
const uint16_t DUTY_MAX = 1023;
const uint16_t DUTY_MIN = 0;

// --- Control PI ---
const float V_REF = 5;     // Voltaje de referencia
const float KP = 190.0;      // Ganancia proporcional
const float KI = 20.0;       // Ganancia integral

// --- ADS1115 ---
Adafruit_ADS1115 ads1; // Primer sensor con dirección 0x48
Adafruit_ADS1115 ads2; // Segundo sensor con dirección 0x49

// Firebase components
UserAuth user_auth(Web_API_KEY, USER_EMAIL, USER_PASS);
FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
RealtimeDatabase Database;

// --- Promedio móvil ---
const int AVG_WINDOW = 20;
int16_t v_cell1_buffer[AVG_WINDOW] = {0};
int16_t v_barra_buffer[AVG_WINDOW] = {0};
int avg_index = 0;
bool avg_filled = false;

int16_t moving_average(int16_t *buffer, int window, bool filled) {
  long sum = 0;
  int count = filled ? window : avg_index;
  if (count == 0) return 0;
  for (int i = 0; i < count; i++) sum += buffer[i];
  return sum / count;
}

// Tareas en Núcleo 1
void CAN_TX_Task(void *pvParameters) {

  twai_message_t message;
  message.identifier = 0x202;
  message.flags = TWAI_MSG_FLAG_NONE;
  message.data_length_code = 8;

  while (true) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
      // Enviar los valores de las celdas y la barra
      message.data[0] = v_cell1 & 0xFF;
      message.data[1] = v_cell1 >> 8;
      message.data[2] = v_barra & 0xFF;
      message.data[3] = v_barra >> 8;
      xSemaphoreGive(dataMutex);
    }
    twai_transmit(&message, pdMS_TO_TICKS(10));
    vTaskDelay(pdMS_TO_TICKS(1000)); // Enviar cada 1 segundo
  }
}

void CAN_RX_Task(void *pvParameters) {
  while (true) {
    twai_message_t message;
    if (twai_receive(&message, pdMS_TO_TICKS(100)) == ESP_OK) {
      if (message.data_length_code == 2) { // SOC promedio recibido
        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
          soc_promedio = (message.data[0] | (message.data[1] << 8));
          xSemaphoreGive(dataMutex);
        }
        Serial.print("SOC promedio recibido: ");
        Serial.println(soc_promedio);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100)); // Esperar 100 ms antes de la siguiente lectura
  }
}

void PWM_Control_Task(void *pvParameters) {

  float voltaje = 0.0;
  float error = 0.0;
  float integral_error = 0.0;
  unsigned long start_time = millis();
  unsigned long end_time = 0;

  while (true) {

    start_time = millis();

    // Leer voltajes de las celdas y actualizar buffers de promedio móvil
    int16_t raw_cell1 = ads1.readADC_Differential_0_3(); // Canal 0-3
    int16_t raw_barra = ads1.readADC_Differential_1_3(); // Canal 1-3
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
      v_cell1_buffer[avg_index] = raw_cell1;
      v_barra_buffer[avg_index] = raw_barra;
      avg_index++;
      if (avg_index >= AVG_WINDOW) {
        avg_index = 0;
        avg_filled = true;
      }
      v_cell1 = moving_average(v_cell1_buffer, AVG_WINDOW, avg_filled);
      v_barra = moving_average(v_barra_buffer, AVG_WINDOW, avg_filled);
      voltaje = v_barra * 2 / 5330.0; // Convertir a voltios
      xSemaphoreGive(dataMutex);
    }

    // --- Controlador PI ---
    error = V_REF - voltaje;
    integral_error += error * 0.05;

    // Calcular señal de control (duty como valor entero entre 0-1023)
    float control = KP * error + KI * integral_error;
    duty = constrain((int)control, DUTY_MIN, DUTY_MAX);

    // Aplicar PWM
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, PWM_CHANNEL, duty);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, PWM_CHANNEL);
    
    // Actualizar tiempo
    end_time = millis();

    vTaskDelay(pdMS_TO_TICKS(50 + start_time - end_time)); // 50 ms de periodo de control
  }
}

void Firebase_Update_Task(void *parameter) {
  JsonWriter writer;
  object_t jsonData, objBarra, objCelda, objTime;
  while(true){
    app.loop(); // Mantener la aplicación Firebase activa
    if (app.ready()) {
      if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        writer.create(objBarra, "/barra", v_barra / 5330.0);
        writer.create(objCelda, "/celda1", v_cell1 / 5330.0);
        writer.create(objTime, "/time", millis() / 1000); // Tiempo en segundos
        xSemaphoreGive(dataMutex);
      }
      writer.join(jsonData, 3, objBarra, objCelda, objTime);
      Database.set<object_t>(aClient, "/Agents/Agent2", jsonData, processData, "🔐 Firebase_Update_Task");
      vTaskDelay(pdMS_TO_TICKS(2000)); // Actualizar cada 2 segundos
    } else {
      vTaskDelay(pdMS_TO_TICKS(1000)); // Espera y reintenta si no está lista
    }
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

  // Inicializar el bus CAN
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK && twai_start() == ESP_OK) {
    Serial.println("CAN inicializado correctamente.");
  } else {
    Serial.println("Error al iniciar CAN.");
  }

  Wire.begin(21, 22); // SDA = 21, SCL = 22

  // Inicializar el primer sensor ADS1115
  if (!ads1.begin(0x48)) {
    Serial.println("Error al inicializar el ADS1115 (0x48)");
  }
  ads1.setGain(GAIN_TWOTHIRDS); // Configurar rango de ±6.144V

  // Inicializar el segundo sensor ADS1115
  if (!ads2.begin(0x49)) {
    Serial.println("Error al inicializar el ADS1115 (0x49)");
  }
  ads2.setGain(GAIN_TWOTHIRDS); // Configurar rango de ±6.144V

  Serial.println("Sensores ADS1115 inicializados correctamente.");

  setupPWM();

  // Crear Mutex
  dataMutex = xSemaphoreCreateMutex();

  // Crear tareas (simulación de paralelismo)
  xTaskCreatePinnedToCore(Firebase_Update_Task, "Firebase_Update", 8192, NULL, 1, NULL, 0); // Núcleo 0
  // xTaskCreatePinnedToCore(CAN_TX_Task, "CAN_TX", 2048, NULL, 2, NULL, 1);  // Núcleo 1
  // xTaskCreatePinnedToCore(CAN_RX_Task, "CAN_RX", 2048, NULL, 2, NULL, 1);  // Núcleo 1
  xTaskCreatePinnedToCore(PWM_Control_Task, "PWM_Control", 4096, NULL, 1, NULL, 1);  // Núcleo 1
}

void loop() {
  // El loop principal queda vacío, ya que las tareas manejan la lógica
}

// --- Configuración PWM ---
void setupPWM() {
  ledc_timer_config_t timerConfig = {
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .duty_resolution = PWM_RESOLUTION,
    .timer_num = PWM_TIMER,
    .freq_hz = freq,
    .clk_cfg = LEDC_AUTO_CLK
  };
  ledc_timer_config(&timerConfig);

  ledc_channel_config_t channelConfig = {
    .gpio_num = PWM_PIN,
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .channel = PWM_CHANNEL,
    .timer_sel = PWM_TIMER,
    .duty = duty,
    .hpoint = 0
  };
  ledc_channel_config(&channelConfig);
}

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