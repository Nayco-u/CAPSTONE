#include "utils.h"

Adafruit_ADS1115 ads1;
Adafruit_ADS1115 ads2;

// --- Variables globales ---
uint16_t local_agent_id = AGENTE_LOCAL;
AgentData agentes_data[MAX_AGENTS] = {};
SemaphoreHandle_t dataMutex = nullptr;
float soc_promedio = 0.0;
uint8_t known_agents[] = {202, 203};
const size_t NUM_AGENTS = sizeof(known_agents) / sizeof(known_agents[0]);

// --- Firebase ---
UserAuth user_auth(Web_API_KEY, USER_EMAIL, USER_PASS);
FirebaseApp app;
WiFiClientSecure ssl_client;
AsyncClient aClient(ssl_client);
RealtimeDatabase Database;

// --- Funciones utilitarias ---
int16_t moving_average(int16_t *buffer, int window, bool filled) {
    int sum = 0;
    int count = filled ? window : 0;
    for (int i = 0; i < window; ++i) sum += buffer[i];
    return count ? sum / count : 0;
}