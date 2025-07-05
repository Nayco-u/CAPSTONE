#include "can_comm.h"

// Variables para los agentes remotos
static uint8_t* target_ids = nullptr;
static size_t target_count = 0;

// --- CAN Initialization ---
void initCAN(uint8_t txPin, uint8_t rxPin) {
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)txPin, (gpio_num_t)rxPin, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK && twai_start() == ESP_OK)
    Serial.println("CAN inicializado correctamente.");
  else
    Serial.println("Error al iniciar CAN.");
}

// --- Transmisión de datos (SOC, corriente) ---
void CAN_TX_Task(void *pvParameters) {
  twai_message_t message;
  message.identifier = local_agent_id;
  message.flags = TWAI_MSG_FLAG_NONE;
  message.data_length_code = 4;

  while (true) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
      message.data[0] = agentes_data[local_agent_id].soc & 0xFF;
      message.data[1] = agentes_data[local_agent_id].soc >> 8;
      message.data[2] = agentes_data[local_agent_id].i_converter & 0xFF;
      message.data[3] = agentes_data[local_agent_id].i_converter >> 8;
      xSemaphoreGive(dataMutex);
    }
    twai_transmit(&message, pdMS_TO_TICKS(10));
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// --- Recepción de datos ---
void CAN_RX_Task(void *pvParameters) {
  while (true) {
    twai_message_t message;
    if (twai_receive(&message, pdMS_TO_TICKS(100)) == ESP_OK) {
      if (message.data_length_code == 4) {
        for (size_t i = 0; i < target_count; ++i) {
          if (message.identifier == target_ids[i]) {
            int16_t soc_recv = message.data[0] | (message.data[1] << 8);
            int16_t curr_recv = message.data[2] | (message.data[3] << 8);

            if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
              agentes_data[message.identifier].soc = soc_recv;
              agentes_data[message.identifier].i_converter = curr_recv;
              agentes_data[message.identifier].last_update = millis();
              xSemaphoreGive(dataMutex);
            }
          }
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// --- Inicializa y lanza las tareas CAN ---
void startCANTasks(uint16_t agent_id) {
  local_agent_id = agent_id;
  target_ids = known_agents;
  target_count = NUM_AGENTS;

  xTaskCreatePinnedToCore(CAN_TX_Task, "CAN_TX", 2048, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(CAN_RX_Task, "CAN_RX", 2048, NULL, 2, NULL, 1);
}
