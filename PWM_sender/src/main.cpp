#include <Arduino.h>
#include "driver/twai.h"

// Pines para CAN
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4

// Variables globales
float Kp = 0.5;          // Ganancia proporcional inicial
float Ki = 0.1;          // Ganancia integral inicial

// Prototipos de funciones
void CAN_TX_Task(void *pvParameters);
void CAN_RX_Task(void *pvParameters);
void Serial_Task(void *pvParameters);

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
    while (true);
  }

  // Crear tareas
  xTaskCreatePinnedToCore(CAN_TX_Task, "CAN_TX", 4096, NULL, 1, NULL, 0);  // Núcleo 0
  xTaskCreatePinnedToCore(CAN_RX_Task, "CAN_RX", 4096, NULL, 1, NULL, 0);  // Núcleo 0
  xTaskCreatePinnedToCore(Serial_Task, "Serial", 4096, NULL, 1, NULL, 1);  // Núcleo 1
}

void loop() {
  // El loop principal queda vacío, ya que las tareas manejan la lógica
  vTaskDelay(pdMS_TO_TICKS(1000));
}

// Tarea para enviar datos por CAN
void CAN_TX_Task(void *pvParameters) {
  while (true) {
    twai_message_t message;
    message.identifier = 0x101; // ID del mensaje
    message.flags = TWAI_MSG_FLAG_NONE;
    message.data_length_code = 2; // Solo se envían Kp y Ki

    // Enviar Kp y Ki
    message.data[0] = static_cast<uint8_t>(Kp * 100); // Convertir a formato fijo
    message.data[1] = static_cast<uint8_t>(Ki * 100); // Convertir a formato fijo

    if (twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK) {
      Serial.println("Kp y Ki enviados por CAN.");
    } else {
      Serial.println("Error al enviar Kp y Ki por CAN.");
    }

    vTaskDelay(pdMS_TO_TICKS(1000)); // Enviar cada 1 segundo
  }
}

// Tarea para recibir datos por CAN
void CAN_RX_Task(void *pvParameters) {
  while (true) {
    twai_message_t message;
    if (twai_receive(&message, pdMS_TO_TICKS(100)) == ESP_OK) {
      if (message.data_length_code == 4) {
        int16_t barraVoltage = message.data[0] | (message.data[1] << 8);
        int duty = message.data[2] | (message.data[3] << 8);

        Serial.print("Voltaje de barra recibido: ");
        Serial.println(barraVoltage);
        Serial.print("Duty cycle recibido: ");
        Serial.println(duty);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// Tarea para manejar la comunicación serial
void Serial_Task(void *pvParameters) {
  while (true) {
    if (Serial.available()) {
      String input = Serial.readStringUntil('\n');
      input.trim();

      if (input.startsWith("KP")) {
        Kp = input.substring(3).toFloat();
        Serial.print("Kp actualizado a: ");
        Serial.println(Kp);
      } else if (input.startsWith("KI")) {
        Ki = input.substring(3).toFloat();
        Serial.print("Ki actualizado a: ");
        Serial.println(Ki);
      } else {
        Serial.println("Comando no reconocido. Use: KP <valor>, KI <valor>");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}