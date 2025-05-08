#include <Arduino.h>
#include "driver/twai.h"

#define MI_ID 0x102
#define CAN_TX_PIN GPIO_NUM_5
#define CAN_RX_PIN GPIO_NUM_4

void setupCAN() {
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK && twai_start() == ESP_OK) {
    Serial.println("CAN inicializado correctamente (Agente 2).");
  } else {
    Serial.println("Error al iniciar CAN.");
  }
}

void receiveCANFrame() {
  twai_message_t message;
  if (twai_receive(&message, pdMS_TO_TICKS(100)) == ESP_OK) {
    if (message.identifier != MI_ID && message.data_length_code == 8) {
      uint16_t soc = message.data[0] | (message.data[1] << 8);
      uint16_t v_celda = message.data[2] | (message.data[3] << 8);
      uint16_t i_salida = message.data[4] | (message.data[5] << 8);
      uint16_t v_barra = message.data[6] | (message.data[7] << 8);

      Serial.println("Agente 2: Mensaje recibido del otro agente:");
      Serial.printf("  SoC: %.2f %%\n", soc / 100.0);
      Serial.printf("  Voltaje Celda: %.3f V\n", v_celda / 1000.0);
      Serial.printf("  Corriente Salida: %.3f A\n", i_salida / 1000.0);
      Serial.printf("  Voltaje Barra: %.3f V\n", v_barra / 1000.0);
    }
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
  receiveCANFrame();
  // checkCANStatus();
  delay(100);  // Pequeño delay para evitar saturar el loop
}
