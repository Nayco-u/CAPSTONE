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
int16_t v_cell2 = 0;       // Voltaje celda 2 en mV
int16_t v_cell3 = 0;       // Voltaje celda 3 en mV
int16_t v_cell4 = 0;       // Voltaje celda 4 en mV
int16_t i_battery = 0;     // Corriente de la batería en mA
int16_t i_converter = 0;   // Corriente del convertidor en mA
int16_t v_barra = 0;       // Voltaje de la barra en mV
int16_t soc_general = 0;   // SOC general del agente (en mV)
int16_t soc_promedio = 0;  // SOC promedio recibido del maestro
SemaphoreHandle_t dataMutex;

// --- Configuración de PWM ---
const int PWM_PIN = 27;
const ledc_channel_t PWM_CHANNEL = LEDC_CHANNEL_0;
const ledc_timer_t PWM_TIMER = LEDC_TIMER_0;
const ledc_timer_bit_t PWM_RESOLUTION = LEDC_TIMER_10_BIT;
int duty = 512;
int freq = 20000;

// --- ADS1115 ---
Adafruit_ADS1115 ads1(0x48); // Primer sensor con dirección 0x48
Adafruit_ADS1115 ads2(0x49); // Segundo sensor con dirección 0x49

// Prototipos de funciones
// Tareas en Núcleo 0
void CAN_TX_Task(void *pvParameters) {
  while (true) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
      // Preparar mensaje CAN con valores simulados
      twai_message_t message;
      message.identifier = 0x102;
      message.flags = TWAI_MSG_FLAG_NONE;
      message.data_length_code = 8;

      // Enviar los valores de las celdas y la barra
      message.data[0] = v_cell1 & 0xFF;
      message.data[1] = v_cell1 >> 8;
      message.data[2] = v_cell2 & 0xFF;
      message.data[3] = v_cell2 >> 8;
      message.data[4] = v_cell3 & 0xFF;
      message.data[5] = v_cell3 >> 8;
      message.data[6] = v_cell4 & 0xFF;
      message.data[7] = v_cell4 >> 8;

      twai_transmit(&message, pdMS_TO_TICKS(1000));

      // Enviar los valores de corriente, barra y SOC general
      message.data[0] = i_battery & 0xFF;
      message.data[1] = i_battery >> 8;
      message.data[2] = i_converter & 0xFF;
      message.data[3] = i_converter >> 8;
      message.data[4] = v_barra & 0xFF;
      message.data[5] = v_barra >> 8;
      message.data[6] = soc_general & 0xFF;
      message.data[7] = soc_general >> 8;

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
  while (true) {
    // Leer voltajes de las celdas
    int16_t rawCell1 = ads2.readADC_SingleEnded(0); // Canal 0
    int16_t rawCell2 = ads2.readADC_SingleEnded(1); // Canal 1
    int16_t rawCell3 = ads2.readADC_SingleEnded(2); // Canal 0
    int16_t rawCell4 = ads2.readADC_SingleEnded(3); // Canal 1

    // Leer corrientes
    int16_t rawBatteryCurrent = ads1.readADC_SingleEnded(0); // Canal 4
    int16_t rawConverterCurrent = ads1.readADC_SingleEnded(1); // Canal 5

    // Leer voltaje de la barra
    int16_t rawBarraVoltage = ads1.readADC_SingleEnded(2); // Canal 6

    // Proteger acceso a variables compartidas
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
      v_cell1 = rawCell1 * 1875 / 10; // Conversión a mV
      v_cell2 = rawCell2 * 1875 / 10; // Conversión a mV
      v_cell3 = rawCell3 * 1875 / 10; // Conversión a mV
      v_cell4 = rawCell4 * 1875 / 10; // Conversión a mV
      i_battery = rawBatteryCurrent * 1875 / 10; // Conversión a mA
      i_converter = rawConverterCurrent * 1875 / 10; // Conversión a mA
      v_barra = rawBarraVoltage * 1875 / 10; // Conversión a mV

      // Calcular el SOC general como el voltaje de la celda más baja
      soc_general = min(min(v_cell1, v_cell2), min(v_cell3, v_cell4));

      xSemaphoreGive(dataMutex);
    }

    delay(500); // Leer cada 500 ms
  }
}

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

// Tareas en Núcleo 1
void PWM_Control_Task(void *pvParameters) {
  int16_t setpoint = 2000;       // Voltaje objetivo en mV
  float Kp = 0.5;               // Ganancia proporcional
  float Ki = 0.1;               // Ganancia integral
  float integral = 0.0;
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

  Wire.begin(2, 15); // SDA = 2, SCL = 15
  setupPWM();

  // Inicializar el primer sensor ADS1115
  if (!ads1.begin()) {
    Serial.println("Error al inicializar el ADS1115 (0x48)");
    while (true);
  }
  ads1.setGain(GAIN_TWOTHIRDS); // Configurar rango de ±6.144V

  // Inicializar el segundo sensor ADS1115
  if (!ads2.begin()) {
    Serial.println("Error al inicializar el ADS1115 (0x49)");
    while (true);
  }
  ads2.setGain(GAIN_TWOTHIRDS); // Configurar rango de ±6.144V

  Serial.println("Sensores ADS1115 inicializados correctamente.");

  // Crear Mutex
  dataMutex = xSemaphoreCreateMutex();

  // Crear tareas (simulación de paralelismo)
  xTaskCreatePinnedToCore(CAN_TX_Task, "CAN_TX", 4096, NULL, 1, NULL, 0);  // Núcleo 0
  xTaskCreatePinnedToCore(CAN_RX_Task, "CAN_RX", 4096, NULL, 1, NULL, 0);  // Núcleo 0
  xTaskCreatePinnedToCore(I2C_Sensor_Task, "I2C_Sensor", 4096, NULL, 1, NULL, 0);  // Núcleo 0

  xTaskCreatePinnedToCore(PWM_Control_Task, "PWM_Control", 4096, NULL, 1, NULL, 1);  // Núcleo 1
  xTaskCreatePinnedToCore(BMS_Control_Task, "BMS_Control", 4096, NULL, 1, NULL, 1);  // Núcleo 1
  xTaskCreatePinnedToCore(MPPT_Control_Task, "MPPT_Control", 4096, NULL, 1, NULL, 1);  // Núcleo 1
}

void loop() {
  // El loop principal queda vacío, ya que las tareas manejan la lógica
  vTaskDelay(pdMS_TO_TICKS(1000));  // Pequeño delay para evitar saturar el loop
}