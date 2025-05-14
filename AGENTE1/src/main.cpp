#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "driver/ledc.h"
#include "driver/twai.h"

// Pines para CAN
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4

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
int duty = 0;
int freq = 20000;
const int DUTY_MAX = 1023;
const int DUTY_MIN = 0;

// --- Control PI ---
const float V_REF = 5;     // Voltaje de referencia
const float KP = 190.0;      // Ganancia proporcional
const float KI = 20.0;       // Ganancia integral
float integral_error = 0.0;
unsigned long last_time = 0;

// --- ADS1115 ---
Adafruit_ADS1115 ads1; // Primer sensor con dirección 0x48
Adafruit_ADS1115 ads2; // Segundo sensor con dirección 0x49

// Prototipos de funciones
// Tareas en Núcleo 0
void CAN_TX_Task(void *pvParameters) {
  while (true) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
      // Preparar mensaje CAN con valores simulados
      twai_message_t message;
      message.identifier = 0x201;
      message.flags = TWAI_MSG_FLAG_NONE;
      message.data_length_code = 8;

      // Enviar los valores de las celdas y la barra
      message.data[0] = v_cell1 & 0xFF;
      message.data[1] = v_cell1 >> 8;
      message.data[2] = v_barra & 0xFF;
      message.data[3] = v_barra >> 8;

      twai_transmit(&message, pdMS_TO_TICKS(1000));

      xSemaphoreGive(dataMutex);
    }
    delay(1000); // Enviar cada 1 segundo
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
    delay(10);
  }
}

void I2C_Sensor_Task(void *pvParameters) {

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

  while (true) {
    // Leer voltajes de las celdas
    int16_t rawCell1 = ads1.readADC_Differential_0_3(); // Canal 0-1
    int16_t rawBarra = ads1.readADC_Differential_1_3(); // Canal 0-3


    // Proteger acceso a variables compartidas
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
      v_cell1 = rawCell1; // Conversión a mV
      v_barra = rawBarra; // Conversión a mV

      xSemaphoreGive(dataMutex);
    }

    delay(500); // Leer cada 500 ms
  }
}

// Tareas en Núcleo 1
void PWM_Control_Task(void *pvParameters) {
  float voltaje = 0.0;
  unsigned long last_time = millis();

  while (true) {
    // Leer el voltaje de la barra (en mV)
    voltaje = v_barra * 2 / 5330.0; // Convertir a voltios


    // --- Controlador PI ---
    float error = V_REF - voltaje;
    unsigned long current_time = millis();
    float dt = (current_time - last_time) / 1000.0;
    last_time = current_time;

    integral_error += error * dt;

    // Calcular señal de control (duty como valor entero entre 0-1023)
    float control = KP * error + KI * integral_error;
    duty = constrain((int)control, DUTY_MIN, DUTY_MAX);

    // Aplicar PWM
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, PWM_CHANNEL, duty);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, PWM_CHANNEL);

      Serial.print("Vmedida: ");
      Serial.print(voltaje, 3);
      Serial.print(" V | Error: ");
      Serial.print(error, 3);
      Serial.print(" Integral | Error: ");
      Serial.print(integral_error, 3);
      Serial.print(" | Duty: ");
      Serial.println(duty);

    vTaskDelay(pdMS_TO_TICKS(500)); // 100 ms de periodo de control
  }
}

void BMS_Control_Task(void *pvParameters) {
  while (true) {
    // Supervisión y control BMS (pendiente)
    delay(500);
  }
}

void MPPT_Control_Task(void *pvParameters) {
  while (true) {
    // Algoritmo MPPT (pendiente)
    delay(500);
  }
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

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Inicializar el bus CAN
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK && twai_start() == ESP_OK) {
    Serial.println("CAN inicializado correctamente.");
  } else {
    Serial.println("Error al iniciar CAN.");
  }

  setupPWM();

  // Crear Mutex
  dataMutex = xSemaphoreCreateMutex();

  // Crear tareas (simulación de paralelismo)
  xTaskCreatePinnedToCore(CAN_TX_Task, "CAN_TX", 4096, NULL, 2, NULL, 0);  // Núcleo 0
  xTaskCreatePinnedToCore(CAN_RX_Task, "CAN_RX", 4096, NULL, 2, NULL, 0);  // Núcleo 0
  xTaskCreatePinnedToCore(I2C_Sensor_Task, "I2C_Sensor", 4096, NULL, 2, NULL, 0);  // Núcleo 0

  xTaskCreatePinnedToCore(PWM_Control_Task, "PWM_Control", 4096, NULL, 1, NULL, 1);  // Núcleo 1
  xTaskCreatePinnedToCore(BMS_Control_Task, "BMS_Control", 4096, NULL, 1, NULL, 1);  // Núcleo 1
  xTaskCreatePinnedToCore(MPPT_Control_Task, "MPPT_Control", 4096, NULL, 1, NULL, 1);  // Núcleo 1
}

void loop() {
  // El loop principal queda vacío, ya que las tareas manejan la lógica
}