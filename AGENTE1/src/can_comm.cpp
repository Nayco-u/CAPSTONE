#include "can_comm.h"

static uint8_t* target_ids = nullptr;
static size_t target_count = 0;
volatile bool message_received = false;

static const TickType_t CAN_WAIT_RESPONSE = pdMS_TO_TICKS(200);

// --- Inicialización del CAN ---
void initCAN(uint8_t txPin, uint8_t rxPin) {
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)txPin, (gpio_num_t)rxPin, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK && twai_start() == ESP_OK)
    Serial.println("CAN inicializado correctamente.");
  else
    Serial.println("Error al iniciar CAN.");
}

// --- Agente maestro: Envía y espera respuesta ---
void CAN_Master_Task(void *pvParameters) {
  twai_message_t message;
  message.identifier = local_agent_id;
  message.flags = TWAI_MSG_FLAG_NONE;
  message.data_length_code = 4;

  while (true) {
    // Enviar mensaje
    if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
      message.data[0] = agentes_data[local_agent_id].soc & 0xFF;
      message.data[1] = agentes_data[local_agent_id].soc >> 8;
      message.data[2] = agentes_data[local_agent_id].i_converter & 0xFF;
      message.data[3] = agentes_data[local_agent_id].i_converter >> 8;
      xSemaphoreGive(dataMutex);
    }
    twai_transmit(&message, pdMS_TO_TICKS(10));
    Serial.println("[MASTER] Mensaje enviado, esperando respuesta...");

    message_received = false;
    TickType_t t_start = xTaskGetTickCount();
    while (!message_received && (xTaskGetTickCount() - t_start < CAN_WAIT_RESPONSE)) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (message_received) {
      Serial.print("[MASTER] Respuesta recibida.");
      Serial.printf(" SOC=%d, I=%d\n", 
        (message.data[0] | (message.data[1] << 8)), 
        (message.data[2] | (message.data[3] << 8)));
    } else {
      Serial.println("[MASTER] No se recibió respuesta.");
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// --- Agente esclavo: Escucha y responde ---
void CAN_Slave_Task(void *pvParameters) {
  while (true) {
    twai_message_t rx_msg;
    if (twai_receive(&rx_msg, pdMS_TO_TICKS(100)) == ESP_OK) {
      if (rx_msg.data_length_code == 4) {
        int16_t soc_recv = rx_msg.data[0] | (rx_msg.data[1] << 8);
        int16_t i_recv = rx_msg.data[2] | (rx_msg.data[3] << 8);

        Serial.printf("[SLAVE] Mensaje recibido de %d | SOC=%d, I=%d\n", rx_msg.identifier, soc_recv, i_recv);

        // Responder
        twai_message_t tx_msg;
        tx_msg.identifier = local_agent_id;
        tx_msg.flags = TWAI_MSG_FLAG_NONE;
        tx_msg.data_length_code = 4;

        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
          tx_msg.data[0] = agentes_data[local_agent_id].soc & 0xFF;
          tx_msg.data[1] = agentes_data[local_agent_id].soc >> 8;
          tx_msg.data[2] = agentes_data[local_agent_id].i_converter & 0xFF;
          tx_msg.data[3] = agentes_data[local_agent_id].i_converter >> 8;
          xSemaphoreGive(dataMutex);
        }
        twai_transmit(&tx_msg, pdMS_TO_TICKS(10));
        Serial.println("[SLAVE] Respuesta enviada.");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// --- RX Task común para monitoreo y detección de mensajes ---
void CAN_Listener_Task(void *pvParameters) {
  while (true) {
    twai_message_t msg;
    if (twai_receive(&msg, pdMS_TO_TICKS(10)) == ESP_OK) {
      message_received = true;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// --- Monitoreo del bus CAN ---
void CAN_Monitor_Task(void *pvParameters) {
  while (true) {
    twai_status_info_t status;
    if (twai_get_status_info(&status) == ESP_OK) {
      if (status.state == TWAI_STATE_BUS_OFF) {
        Serial.println("[CAN] BUS OFF detectado. Reiniciando controlador CAN...");
        twai_stop();
        twai_driver_uninstall();
        delay(100);
        initCAN(CAN_TX_PIN, CAN_RX_PIN);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

// --- Inicializa tareas según rol ---
void startCANMaster(uint16_t agent_id) {
  local_agent_id = agent_id;
  xTaskCreatePinnedToCore(CAN_Master_Task, "CAN_Master", 2048, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(CAN_Listener_Task, "CAN_Listener", 2048, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(CAN_Monitor_Task, "CAN_Monitor", 2048, NULL, 1, NULL, 1);
}

void startCANSlave(uint16_t agent_id) {
  local_agent_id = agent_id;
  xTaskCreatePinnedToCore(CAN_Slave_Task, "CAN_Slave", 2048, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(CAN_Monitor_Task, "CAN_Monitor", 2048, NULL, 1, NULL, 1);
}