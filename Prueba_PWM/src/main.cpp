#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include "driver/ledc.h"
#include "driver/twai.h"

// Pines para CAN
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4

// Variables globales compartidas
int16_t v_barra = 0;       // Voltaje de la barra en mV
float Kp = 0.5;            // Ganancia proporcional (recibida por CAN)
float Ki = 0.1;            // Ganancia integral (recibida por CAN)
float integral = 0.0;      // Parte integral del controlador
int duty = 512;            // Duty cycle del PWM
SemaphoreHandle_t dataMutex;

// --- Configuración de PWM ---
const int PWM_PIN = 27;
const ledc_channel_t PWM_CHANNEL = LEDC_CHANNEL_0;
const ledc_timer_t PWM_TIMER = LEDC_TIMER_0;
const ledc_timer_bit_t PWM_RESOLUTION = LEDC_TIMER_10_BIT;
int freq = 20000;

// --- Configuración del ADS1115 ---
Adafruit_ADS1115 ads(0x48); // Dirección I2C del sensor

// Prototipos de funciones
void CAN_TX_Task(void *pvParameters);
void CAN_RX_Task(void *pvParameters);
void PWM_Control_Task(void *pvParameters);
void I2C_Sensor_Task(void *pvParameters);

// --- Configuración de PWM ---
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

// Tarea para leer el voltaje del sensor ADS1115
void I2C_Sensor_Task(void *pvParameters) {
  while (true) {
    // Leer el voltaje de la barra desde el sensor ADS1115
    int16_t rawVoltage = ads.readADC_SingleEnded(0); // Leer del canal 0
    int16_t barraVoltage = rawVoltage * 0.1875; // Convertir a mV (0.1875 mV por bit)

    // Proteger acceso a la variable compartida
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
      v_barra = barraVoltage;
      xSemaphoreGive(dataMutex);
    }

    delay(500); // Leer cada 500 ms
  }
}

// Tarea para enviar datos por CAN
void CAN_TX_Task(void *pvParameters) {
  while (true) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
      // Preparar mensaje CAN
      twai_message_t message;
      message.identifier = 0x101;
      message.flags = TWAI_MSG_FLAG_NONE;
      message.data_length_code = 4;

      // Enviar el voltaje de la barra y el duty cycle
      message.data[0] = v_barra & 0xFF;
      message.data[1] = v_barra >> 8;
      message.data[2] = duty & 0xFF;
      message.data[3] = duty >> 8;

      twai_transmit(&message, pdMS_TO_TICKS(1000));
      xSemaphoreGive(dataMutex);
    }
    delay(1000); // Enviar cada 1 segundo
  }
}

// Tarea para recibir datos por CAN
void CAN_RX_Task(void *pvParameters) {
  while (true) {
    twai_message_t message;
    if (twai_receive(&message, pdMS_TO_TICKS(100)) == ESP_OK) {
      if (message.data_length_code == 4) { // Recibir Kp y Ki
        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
          Kp = ((message.data[0] | (message.data[1] << 8)) / 100.0); // Kp en formato fijo
          Ki = ((message.data[2] | (message.data[3] << 8)) / 100.0); // Ki en formato fijo
          xSemaphoreGive(dataMutex);
        }
        Serial.print("Kp recibido: ");
        Serial.println(Kp);
        Serial.print("Ki recibido: ");
        Serial.println(Ki);
      }
    }
    delay(10);
  }
}

// Tarea para el control PID
void PWM_Control_Task(void *pvParameters) {
  int16_t setpoint = 2000;       // Voltaje objetivo en mV
  float integral_max = 1000.0;  // Límite del integral
  float dt = 0.5;               // Intervalo de muestreo en segundos

  while (true) {
    int16_t barraVoltage;

    // Leer el voltaje de la barra
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
      barraVoltage = v_barra;
      xSemaphoreGive(dataMutex);
    }

    // Calcular el error basado en el voltaje de la barra
    int16_t error = setpoint - barraVoltage;

    // Control PI
    integral += error * dt;
    integral = constrain(integral, -integral_max, integral_max);  // Limitar integral
    int16_t output = Kp * error + Ki * integral;

    // Aplicar al PWM
    duty = constrain(output, 0, 1023);
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, PWM_CHANNEL, duty);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, PWM_CHANNEL);

    delay(static_cast<int>(dt * 1000));
  }
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

  // Inicializar el sensor ADS1115
  if (!ads.begin()) {
    Serial.println("Error al inicializar el ADS1115.");
    while (true);
  }
  ads.setGain(GAIN_TWOTHIRDS); // Configurar rango de ±6.144V

  setupPWM();

  // Crear Mutex
  dataMutex = xSemaphoreCreateMutex();

  // Crear tareas
  xTaskCreatePinnedToCore(CAN_TX_Task, "CAN_TX", 4096, NULL, 1, NULL, 0);  // Núcleo 0
  xTaskCreatePinnedToCore(CAN_RX_Task, "CAN_RX", 4096, NULL, 1, NULL, 0);  // Núcleo 0
  xTaskCreatePinnedToCore(I2C_Sensor_Task, "I2C_Sensor", 4096, NULL, 1, NULL, 0);  // Núcleo 0
  xTaskCreatePinnedToCore(PWM_Control_Task, "PWM_Control", 4096, NULL, 1, NULL, 1);  // Núcleo 1
}

void loop() {
  // El loop principal queda vacío, ya que las tareas manejan la lógica
  vTaskDelay(pdMS_TO_TICKS(1000));  // Pequeño delay para evitar saturar el loop
}