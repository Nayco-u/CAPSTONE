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
#define WIFI_SSID "MRI_WIRELESS2"
#define WIFI_PASSWORD "2718281828"

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
int16_t soc = 0;           // Estado de carga (SOC) en %
int16_t soc_promedio = 0;  // SOC promedio recibido del maestro
int16_t soc_agente2 = 0;
int16_t soc_agente3 = 0;

unsigned long agente2_last_update = 0; // Última actualización de agente 2
unsigned long agente3_last_update = 0; // Última actualización de agente 3

SemaphoreHandle_t dataMutex;

// --- Configuración de PWM ---
const int PWM_PIN = 27;
const ledc_channel_t PWM_CHANNEL = LEDC_CHANNEL_0;
const ledc_timer_t PWM_TIMER = LEDC_TIMER_0;
const ledc_timer_bit_t PWM_RESOLUTION = LEDC_TIMER_10_BIT; // 10 bits = 1024 niveles
uint16_t duty = 0;
const uint16_t freq = 20000;
const uint16_t DUTY_MAX = 580; // Ajustado para un rango de 0-580
const uint16_t DUTY_MIN = 0;

// --- Control PI ---
float V_REF = 5;     // Voltaje de referencia
const float KP_1 = 5.0;      // Ganancia proporcional
const float KI_1 = 10.0;       // Ganancia integral

const float KP_2 = 5.0;      // Ganancia proporcional
const float KI_2 = 10.0;       // Ganancia integral

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
const int AVG_WINDOW = 10;
int16_t v_cell1_buffer[AVG_WINDOW] = {0};
int16_t v_barra_buffer[AVG_WINDOW] = {0};
int16_t i_battery_buffer[AVG_WINDOW] = {0};
int16_t i_converter_buffer[AVG_WINDOW] = {0};
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
  message.identifier = 0x201;
  message.flags = TWAI_MSG_FLAG_NONE;
  message.data_length_code = 2; // Solo SOC

  while (true) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
      message.data[0] = soc & 0xFF;
      message.data[1] = soc >> 8;
      xSemaphoreGive(dataMutex);
    }
    twai_transmit(&message, pdMS_TO_TICKS(10));
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void CAN_RX_Task(void *pvParameters) {
  while (true) {
    twai_message_t message;
    if (twai_receive(&message, pdMS_TO_TICKS(100)) == ESP_OK) {
      if (message.data_length_code == 2) {
        int16_t soc_recibido = (message.data[0] | (message.data[1] << 8));
        if (message.identifier == 0x202) {
          soc_agente2 = soc_recibido;
          agente2_last_update = millis(); // Actualizar tiempo de última recepción
        } else if (message.identifier == 0x203) {
          soc_agente3 = soc_recibido;
          agente3_last_update = millis(); // Actualizar tiempo de última recepción
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void PWM_Control_Task(void *pvParameters) {

  float voltaje = 0.0;
  float corriente = 0.0;
  float error_v = 0.0;
  float integral_error_v = 0.0;

  float error_i = 0.0;
  float integral_error_i = 0.0;

  unsigned long start_time = millis();
  unsigned long end_time = 0;

  int16_t raw_cell1 = 0;
  int16_t raw_barra = 0;
  int16_t raw_batery = 0;
  int16_t raw_converter = 0;

  bool bias_calculated = false;
  int16_t i_battery_bias = 0;
  int16_t i_converter_bias = 0;

  while(!bias_calculated) {
    raw_cell1 = ads1.readADC_Differential_0_3(); // Canal 0-3
    raw_batery = ads2.readADC_Differential_0_3(); // Canal 0-3
    raw_converter = ads2.readADC_Differential_1_3(); // Canal 1-3

    v_cell1_buffer[avg_index] = raw_cell1;
    i_battery_buffer[avg_index] = raw_batery;
    i_converter_buffer[avg_index] = raw_converter;
    avg_index++;
    if (avg_index >= AVG_WINDOW) {
      avg_index = 0;
      avg_filled = true;
    }
    soc = moving_average(v_cell1_buffer, AVG_WINDOW, avg_filled) * 1.641651 - 26000;
    i_battery_bias = moving_average(i_battery_buffer, AVG_WINDOW, avg_filled);
    i_converter_bias = moving_average(i_converter_buffer, AVG_WINDOW, avg_filled);

    if (avg_filled){
      bias_calculated = true;
      avg_filled = false;
      avg_index = 0; // Reiniciar el índice para el siguiente ciclo de promedio
      for (int i = 0; i < AVG_WINDOW; i++) {
        v_cell1_buffer[i] = 0;
        i_battery_buffer[i] = 0;
        i_converter_buffer[i] = 0;
      }
      Serial.println("Bias calculado: ");
      Serial.print("i_battery_bias: ");
      Serial.println(i_battery_bias);
      Serial.print("i_converter_bias: ");
      Serial.println(i_converter_bias);
    } else {
      vTaskDelay(pdMS_TO_TICKS(20)); // Esperar 20 ms antes de la siguiente lectura
    };
  };

  while (true) {

    start_time = millis();

    // Leer voltajes de las celdas y actualizar buffers de promedio móvil
    raw_cell1 = ads1.readADC_Differential_0_3(); // Canal 0-3
    raw_barra = ads1.readADC_Differential_1_3(); // Canal 1-3
    raw_batery = ads2.readADC_Differential_0_3(); // Canal 0 -3
    raw_converter = ads2.readADC_Differential_1_3(); // Canal 1-3

    v_cell1_buffer[avg_index] = raw_cell1;
    v_barra_buffer[avg_index] = raw_barra;
    i_battery_buffer[avg_index] = raw_batery;
    i_converter_buffer[avg_index] = raw_converter;
    avg_index++;
    if (avg_index >= AVG_WINDOW) {
      avg_index = 0;
      avg_filled = true;
    }

    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {          
      v_cell1 = moving_average(v_cell1_buffer, AVG_WINDOW, avg_filled);
      v_barra = moving_average(v_barra_buffer, AVG_WINDOW, avg_filled);
      i_battery = moving_average(i_battery_buffer, AVG_WINDOW, avg_filled) - i_battery_bias;
      i_converter = moving_average(i_converter_buffer, AVG_WINDOW, avg_filled) - i_converter_bias;
      xSemaphoreGive(dataMutex);
    }
    soc += i_battery * 720000 * 0.0001875; // Actualizar SOC basado en la corriente de la batería
    voltaje = v_barra * 0.0001875; // Convertir a voltios
    corriente = i_battery * 0.0001875 * 10; // Convertir a amperios

    int agentes_activos = 1; // Siempre este agente está activo
    int32_t suma_soc = soc;

    if (start_time - agente2_last_update < 2000) { // 2 segundos
        suma_soc += soc_agente2;
        agentes_activos++;
    }
    if (start_time - agente3_last_update < 2000) {
        suma_soc += soc_agente3;
        agentes_activos++;
    }

    soc_promedio = suma_soc / agentes_activos; // Promedio de SOC de los agentes
    int16_t delta_soc = soc_promedio - soc; // Diferencia entre SOC local y promedio

    // --- Controlador PI ---
    error_v = V_REF - voltaje - (delta_soc * 0.0001); // Ajustar error con delta SOC
    integral_error_v += error_v * 0.05;

    float i_ref = KP_1 * error_v + KI_1 * integral_error_v;

    error_i = i_ref - corriente; // Error de corriente
    integral_error_i += error_i * 0.05;

    float control = KP_2 * error_i + KI_2 * integral_error_i;
    duty = constrain((int)control, DUTY_MIN, DUTY_MAX);

    // Aplicar PWM
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, PWM_CHANNEL, duty);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, PWM_CHANNEL);

    // Imprimir valores para depuración
    Serial.print("Voltaje: ");
    Serial.print(voltaje, 2);
    Serial.print(" V, SOC: ");
    Serial.print(soc / 100, 3);
    Serial.print("%, Corriente Batería: ");
    Serial.print(i_battery * 0.0001875 * 10, 3);
    Serial.print(" V, Duty: ");
    Serial.println(duty);
    
    // Actualizar tiempo
    end_time = millis();
    int delta = 50 + start_time - end_time;
    vTaskDelay(pdMS_TO_TICKS(max(0, delta)));
  }
}

void Firebase_Update_Task(void *parameter) {
  JsonWriter writer;
  object_t jsonData, objBarra, objCelda, objBattery, objIBarra, objTime;
  while(true){
    app.loop(); // Mantener la aplicación Firebase activa
    if (app.ready()) {
      if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        writer.create(objBarra, "/barra", v_barra * 0.0001875);
        writer.create(objCelda, "/celda1", v_cell1 * 0.0001875);
        writer.create(objBattery, "/battery", i_battery * 0.0001875 * 10); // Corriente de la batería en A
        writer.create(objIBarra, "/ibar", i_converter * 0.0001875 * 10); // Corriente del convertidor en A
        writer.create(jsonData, "/soc", soc / 100.0); // SOC en porcentaje
        writer.create(objTime, "/time", millis() / 1000); // Tiempo en segundos
        xSemaphoreGive(dataMutex);
      }
      writer.join(jsonData, 3, objBarra, objCelda, objBattery, objIBarra, objTime);
      Database.set<object_t>(aClient, "/Agents/Agent1", jsonData, processData, "🔐 Firebase_Update_Task");
      vTaskDelay(pdMS_TO_TICKS(5000)); // Actualizar cada 5 segundos
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