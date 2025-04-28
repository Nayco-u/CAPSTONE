#include <Arduino.h>
#include "driver/twai.h"

// Pines para CAN
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4

// Variables globales compartidas
float voltage = 0.0;
float current = 0.0;
float soc = 0.0;
float barra_voltage = 0.0;
SemaphoreHandle_t dataMutex;

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

      uint16_t soc_val = soc * 100; // Porcentaje a centésimas
      uint16_t v_cell = voltage * 1000; // Volts a mV
      uint16_t i_out = current * 1000;  // Amps a mA
      uint16_t v_barra = barra_voltage * 1000; // Volts a mV

      message.data[0] = soc_val & 0xFF;
      message.data[1] = soc_val >> 8;
      message.data[2] = v_cell & 0xFF;
      message.data[3] = v_cell >> 8;
      message.data[4] = i_out & 0xFF;
      message.data[5] = i_out >> 8;
      message.data[6] = v_barra & 0xFF;
      message.data[7] = v_barra >> 8;

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
      if (message.data_length_code == 8) {
        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
          soc = (message.data[0] | (message.data[1] << 8)) / 100.0;
          voltage = (message.data[2] | (message.data[3] << 8)) / 1000.0;
          current = (message.data[4] | (message.data[5] << 8)) / 1000.0;
          barra_voltage = (message.data[6] | (message.data[7] << 8)) / 1000.0;
          xSemaphoreGive(dataMutex);
        }
        Serial.println("Datos CAN actualizados.");
      }
    }
    delay(10);
  }
}

void I2C_Sensor_Task(void *pvParameters) {
  while (true) {
    // Leer sensores I2C aquí (pendiente)
    delay(500);
  }
}

// Tareas en Núcleo 1
void PWM_Control_Task(void *pvParameters) {
  while (true) {
    // Control PWM conversor DC-DC (pendiente)
    delay(100);
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