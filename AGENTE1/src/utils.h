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
#define CAN_TX_PIN GPIO_NUM_4
#define MAX_AGENTS 204
#define AGENTE_LOCAL 201
#define AVG_WINDOW 80

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
#define KP_1              1.6
#define KI_1              20.0
#define KP_2              2.0  //2.0
#define KI_2              0.005 // 0.005

// Control por SOC
#define KP_3              16
#define KI_3              200.0

// Referencia
#define V_REF             5.0

// Configuración del pin PWM para el ventilador
#define FAN_PWM_PIN      12
#define FAN_PWM_CHANNEL  LEDC_CHANNEL_1
#define FAN_PWM_TIMER    LEDC_TIMER_1
#define FAN_PWM_FREQ     20000
#define FAN_PWM_RES      LEDC_TIMER_8_BIT

// Pines de control (ajusta según tu hardware)
#define RELAY_PIN      25
#define TRANSISTOR_PIN 26

// Umbrales de SOC (ajusta según tu batería)
#define SOC_MIN 2000   // 20%
#define SOC_MAX 8000   // 80%

// --- Credenciales WiFi y Firebase ---
#define WIFI_SSID "naico_wifi"
#define WIFI_PASSWORD "12345678"
#define Web_API_KEY "AIzaSyAVt_Gn_2jqQYufbcg4GzsJ6Q6MoozBGYU"
#define DATABASE_URL "https://capstone-b36f2-default-rtdb.firebaseio.com/"
#define USER_EMAIL "capstone.potencia.2025@gmail.com"
#define USER_PASS "capstone2025"

// Pines LED RGB
#define LED_R_PIN 14
#define LED_G_PIN 32
#define LED_B_PIN 33

// PWM para colores (cátodo común, brillo proporcional al duty)
#define LEDC_TIMER_STATUS  LEDC_TIMER_2
#define LEDC_MODE_STATUS   LEDC_HIGH_SPEED_MODE
#define LEDC_RESOLUTION    LEDC_TIMER_8_BIT
#define LEDC_FREQ_STATUS   5000

#define LEDC_CH_R LEDC_CHANNEL_2
#define LEDC_CH_G LEDC_CHANNEL_3
#define LEDC_CH_B LEDC_CHANNEL_4

// --- Funciones utilitarias ---
int16_t moving_average(int16_t *buffer, int window, bool filled);

// Calcula el SOC (0-10000) a partir del voltaje de celda (en voltios)
int16_t soc_from_voltage(float v_celda);

// Funciones
void setupStatusLED();
void setLEDColor(uint8_t r, uint8_t g, uint8_t b);