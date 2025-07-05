#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#pragma once
#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include <Wire.h>
#include <FirebaseClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <driver/ledc.h>

// --- Parámetros generales ---
#define MAX_AGENTS 204
#define AGENTE_LOCAL 201
#define AVG_WINDOW 10

// Pines para CAN y botón de parada
#define CAN_TX_PIN GPIO_NUM_4
#define CAN_RX_PIN GPIO_NUM_5

// --- Estructura de datos de los agentes ---
typedef struct {
    int16_t soc;
    int16_t i_converter;
    int16_t v_barra;
    int16_t v_cell1;
    int16_t i_battery;
    unsigned long last_update;
    unsigned long time;
    // Puedes agregar más campos si lo necesitas
} AgentData;

// --- Variables globales ---
extern uint16_t local_agent_id;
extern AgentData agentes_data[MAX_AGENTS];
extern SemaphoreHandle_t dataMutex;
extern float soc_promedio;
extern uint8_t known_agents[];
extern const size_t NUM_AGENTS;

extern Adafruit_ADS1115 ads1;
extern Adafruit_ADS1115 ads2;

// --- Firebase ---
extern UserAuth user_auth;
extern FirebaseApp app;
extern WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
extern AsyncClient aClient;
extern RealtimeDatabase Database;

// --- Parámetros de PWM y Control ---
#define PWM_PIN           27
#define PWM_MODE          LEDC_HIGH_SPEED_MODE
#define PWM_CHANNEL       LEDC_CHANNEL_0
#define PWM_TIMER         LEDC_TIMER_0
#define PWM_RESOLUTION    LEDC_TIMER_10_BIT
#define PWM_FREQ_HZ       20000

#define DUTY_MAX          580  // 61.8% de 1023 (modo boost)
#define DUTY_MIN          0

// Control PI (Voltaje → corriente → PWM)
#define KP_1              5.0
#define KI_1              10.0
#define KP_2              5.0
#define KI_2              10.0

// Control por SOC
#define KP_3              0.1
#define KI_3              0.1

// Referencia
#define V_REF             5.0

// Configuración del pin PWM para el ventilador
#define FAN_PWM_PIN      26
#define FAN_PWM_CHANNEL  LEDC_CHANNEL_1
#define FAN_PWM_TIMER    LEDC_TIMER_1
#define FAN_PWM_FREQ     20000
#define FAN_PWM_RES      LEDC_TIMER_8_BIT

// Pines de control (ajusta según tu hardware)
#define RELAY_PIN      25
#define TRANSISTOR_PIN 33

// Umbrales de SOC (ajusta según tu batería)
#define SOC_MIN 2000   // 20%
#define SOC_MAX 9000   // 90%

// --- Credenciales WiFi y Firebase ---
#define WIFI_SSID "Red_Capstone"
#define WIFI_PASSWORD "kkg9-nbfu-297p"
#define Web_API_KEY "AIzaSyAVt_Gn_2jqQYufbcg4GzsJ6Q6MoozBGYU"
#define DATABASE_URL "https://capstone-b36f2-default-rtdb.firebaseio.com/"
#define USER_EMAIL "capstone.potencia.2025@gmail.com"
#define USER_PASS "capstone2025"

// --- Funciones utilitarias ---
int16_t moving_average(int16_t *buffer, int window, bool filled);
