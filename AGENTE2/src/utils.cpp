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
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
UserAuth user_auth(Web_API_KEY, USER_EMAIL, USER_PASS);
FirebaseApp app;
RealtimeDatabase Database;

// --- Funciones utilitarias ---
int16_t moving_average(int16_t *buffer, int window, bool filled) {
    int sum = 0;
    int count = filled ? window : 0;
    for (int i = 0; i < window; ++i) sum += buffer[i];
    return count ? sum / count : 0;
}

int16_t soc_from_voltage(float v_celda) {
    float soc;
    if (v_celda <= 3.2) {
        soc = 0.2 * (v_celda - 3.0) / 0.2;
    } else if (v_celda <= 4.0) {
        soc = 0.2 + 0.7 * (v_celda - 3.2) / 0.8;
    } else {
        soc = 0.9 + 0.1 * (v_celda - 4.0) / 0.2;
    }
    soc = constrain(soc, 0.0, 1.0);
    return (int16_t)(soc * 10000);
}

void setupStatusLED() {
  ledcSetup(LEDC_CH_R, LEDC_FREQ_STATUS, 8);
  ledcSetup(LEDC_CH_G, LEDC_FREQ_STATUS, 8);
  ledcSetup(LEDC_CH_B, LEDC_FREQ_STATUS, 8);

  ledcAttachPin(LED_R_PIN, LEDC_CH_R);
  ledcAttachPin(LED_G_PIN, LEDC_CH_G);
  ledcAttachPin(LED_B_PIN, LEDC_CH_B);

  setLEDColor(255, 255, 0); // Azul al inicio
}

void setLEDColor(uint8_t r, uint8_t g, uint8_t b) {
  ledcWrite(LEDC_CH_R, r);
  ledcWrite(LEDC_CH_G, g);
  ledcWrite(LEDC_CH_B, b);
}