#include <Arduino.h>
#include "driver/twai.h"

#define AGENTE_ID 0x101
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4

void setupCAN() {
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK && twai_start() == ESP_OK) {
    Serial.println("CAN inicializado correctamente (Agente 1).");
  } else {
    Serial.println("Error al iniciar CAN.");
  }
}

void sendCANFrame() {
  uint16_t soc = 7300;
  uint16_t v_celda = 3820;
  uint16_t i_salida = 1250;
  uint16_t v_barra = 5010;

  twai_message_t message;
  message.identifier = AGENTE_ID;
  message.flags = TWAI_MSG_FLAG_NONE;
  message.data_length_code = 8;

  message.data[0] = soc & 0xFF;
  message.data[1] = soc >> 8;
  message.data[2] = v_celda & 0xFF;
  message.data[3] = v_celda >> 8;
  message.data[4] = i_salida & 0xFF;
  message.data[5] = i_salida >> 8;
  message.data[6] = v_barra & 0xFF;
  message.data[7] = v_barra >> 8;

  if (twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK) {
    Serial.println("Agente 1: Mensaje enviado.");
  } else {
    Serial.println("Agente 1: Error al enviar.");
  }
}

void checkCANStatus() {
  twai_status_info_t status;
  twai_get_status_info(&status);
  Serial.printf("Estado del bus: %d, errores TX: %d, RX: %d, mensajes TX: %d\n",
                status.state, status.tx_error_counter, status.rx_error_counter, status.msgs_to_tx);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  setupCAN();
}

void loop() {
  sendCANFrame();
  // checkCANStatus();
  delay(2000);
}
