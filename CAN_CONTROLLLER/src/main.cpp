#include <Arduino.h>
#include "driver/twai.h"

// Pines del transceptor TJA1050 (puedes cambiarlos si es necesario)
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4

void setupCAN() {
  // Configuración general
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
  // Configuración de tiempo (velocidad 500 kbps)
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  // Sin filtros
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  // Instanciar el controlador
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK &&
      twai_start() == ESP_OK) {
    Serial.println("CAN inicializado correctamente.");
  } else {
    Serial.println("Error al iniciar CAN.");
  }
}

void sendCANFrame() {
  // Datos simulados
  uint16_t soc = 7300;             // 73.00 % (por ejemplo, en centésimas)
  uint16_t v_celda = 3820;         // 3.820 V = 3820 mV
  uint16_t i_salida = 1250;        // 1.250 A = 1250 mA
  uint16_t v_barra = 5010;         // 5.010 V = 5010 mV

  // Armar el mensaje
  twai_message_t message;
  message.identifier = 0x101;
  message.flags = TWAI_MSG_FLAG_NONE;
  message.data_length_code = 8;

  // Insertar los datos en el arreglo de bytes (Little Endian)
  message.data[0] = soc & 0xFF;
  message.data[1] = soc >> 8;
  message.data[2] = v_celda & 0xFF;
  message.data[3] = v_celda >> 8;
  message.data[4] = i_salida & 0xFF;
  message.data[5] = i_salida >> 8;
  message.data[6] = v_barra & 0xFF;
  message.data[7] = v_barra >> 8;

  if (twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK) {
    Serial.println("Mensaje CAN enviado con datos simulados.");
  } else {
    Serial.println("Fallo al enviar.");
  }
}


void receiveCANFrame() {
  twai_message_t message;
  if (twai_receive(&message, pdMS_TO_TICKS(100)) == ESP_OK) {
    if (message.data_length_code == 8) {
      uint16_t soc = message.data[0] | (message.data[1] << 8);
      uint16_t v_celda = message.data[2] | (message.data[3] << 8);
      uint16_t i_salida = message.data[4] | (message.data[5] << 8);
      uint16_t v_barra = message.data[6] | (message.data[7] << 8);

      Serial.println("Mensaje CAN recibido:");
      Serial.printf("  SoC: %.2f %%\n", soc / 100.0);
      Serial.printf("  Voltaje Celda: %.3f V\n", v_celda / 1000.0);
      Serial.printf("  Corriente Salida: %.3f A\n", i_salida / 1000.0);
      Serial.printf("  Voltaje Barra: %.3f V\n", v_barra / 1000.0);
    } else {
      Serial.println("Mensaje recibido con longitud inesperada.");
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  setupCAN();
}

void loop() {
  sendCANFrame();
  receiveCANFrame();
  delay(1000);  // Esperar 1 segundo entre mensajes
}