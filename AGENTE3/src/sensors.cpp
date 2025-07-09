#include "sensors.h"

// === Buffers para promedios ===
static int16_t v_cell1_buffer[AVG_WINDOW] = {0};
static int16_t v_cell2_buffer[AVG_WINDOW] = {0};
static int16_t v_panel_buffer[AVG_WINDOW]  = {0};
static int16_t v_barra_buffer[AVG_WINDOW] = {0};
static int16_t i_battery_buffer[AVG_WINDOW] = {0};
static int16_t i_converter_buffer[AVG_WINDOW] = {0};
static int16_t i_panel_buffer[AVG_WINDOW] = {0};

// === Bias ===
static int16_t i_battery_bias = 0;
static int16_t i_converter_bias = 0;
static int16_t i_panel_bias = 0;

// === Estado del promedio ===
static int avg_index = 0;
static bool avg_filled = false;

static int16_t moving_average(int16_t *buffer) {
  int sum = 0;
  int count = avg_filled ? AVG_WINDOW : avg_index;
  if (count == 0) return 0;
  for (int i = 0; i < count; ++i) sum += buffer[i];
  return sum / count;
}

void initADS() {
  Wire.begin(21, 22);
  while (!ads1.begin(0x48)) {
    Serial.println("Error al inicializar ADS1, reintentando...");
    delay(500);
  }
  ads1.setGain(GAIN_TWOTHIRDS);
  Serial.println("ADS1 inicializado.");

  while (!ads2.begin(0x49)) {
    Serial.println("Error al inicializar ADS2, reintentando...");
    delay(500);
  }
  ads2.setGain(GAIN_TWOTHIRDS);
  Serial.println("ADS2 inicializado.");
  while (!ads3.begin(0x4A)) {
    Serial.println("Error al inicializar ADS3, reintentando...");
    delay(500);
  }
  ads3.setGain(GAIN_TWOTHIRDS);
  Serial.println("ADS3 inicializado.");
}

void calibrarBias() {
  avg_index = 0;
  avg_filled = false;
  for (int i = 0; i < AVG_WINDOW; ++i) {
    v_cell1_buffer[i] = 0;
    v_cell2_buffer[i] = 0;
    v_panel_buffer[i] = 0;
    v_barra_buffer[i] = 0;
    i_battery_buffer[i] = 0;
    i_converter_buffer[i] = 0;
    i_panel_buffer[i] = 0;
  }

  while (!avg_filled) {
    int16_t raw_cell1     = ads1.readADC_Differential_1_3();
    int16_t raw_cell2     = ads3.readADC_Differential_0_1();
    int16_t raw_panel_v   = ads1.readADC_Differential_2_3();
    int16_t raw_barra     = ads1.readADC_Differential_0_3();
    int16_t raw_battery   = ads2.readADC_Differential_0_3();
    int16_t raw_converter = ads2.readADC_Differential_2_3();
    int16_t raw_panel_i   = ads2.readADC_Differential_1_3();

    v_cell1_buffer[avg_index] = raw_cell1;
    v_cell2_buffer[avg_index] = raw_cell2;
    v_panel_buffer[avg_index] = raw_panel_v;
    v_barra_buffer[avg_index] = raw_barra;
    i_battery_buffer[avg_index] = raw_battery;
    i_converter_buffer[avg_index] = raw_converter;
    i_panel_buffer[avg_index] = raw_panel_i;

    avg_index++;
    if (avg_index >= AVG_WINDOW) {
      avg_index = 0;
      avg_filled = true;
    }
    delay(5);
  }

  if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
    agentes_data[local_agent_id].v_cell1 = moving_average(v_cell1_buffer);
    agentes_data[local_agent_id].v_cell2 = moving_average(v_cell2_buffer);
    agentes_data[local_agent_id].v_panel = moving_average(v_panel_buffer);
    i_battery_bias    = moving_average(i_battery_buffer);
    i_converter_bias  = moving_average(i_converter_buffer);
    i_panel_bias      = moving_average(i_panel_buffer);
    xSemaphoreGive(dataMutex);
  }

  Serial.println("Bias calibrado.");
  Serial.print("i_battery_bias: "); Serial.println(ads2.computeVolts(i_battery_bias), 5);
  Serial.print("i_converter_bias: "); Serial.println(ads2.computeVolts(i_converter_bias), 5);
  Serial.print("i_panel_bias: "); Serial.println(ads2.computeVolts(i_panel_bias), 5);
}

void leerSensores() {
  int16_t raw_cell1     = ads1.readADC_Differential_1_3();
  int16_t raw_cell2     = ads3.readADC_Differential_0_1();
  int16_t raw_panel_v   = ads1.readADC_Differential_2_3();
  int16_t raw_barra     = ads1.readADC_Differential_0_3();
  int16_t raw_battery   = ads2.readADC_Differential_0_3();
  int16_t raw_panel_i   = ads2.readADC_Differential_1_3();
  int16_t raw_converter = ads2.readADC_Differential_2_3();

  v_cell1_buffer[avg_index] = raw_cell1;
  v_cell2_buffer[avg_index] = raw_cell2;
  v_panel_buffer[avg_index] = raw_panel_v;
  v_barra_buffer[avg_index] = raw_barra;
  i_battery_buffer[avg_index] = raw_battery;
  i_converter_buffer[avg_index] = raw_converter;
  i_panel_buffer[avg_index] = raw_panel_i;

  if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
    agentes_data[local_agent_id].v_cell1 = moving_average(v_cell1_buffer);
    agentes_data[local_agent_id].v_cell2 = moving_average(v_cell2_buffer);
    agentes_data[local_agent_id].v_panel = moving_average(v_panel_buffer);
    agentes_data[local_agent_id].v_barra = moving_average(v_barra_buffer);
    agentes_data[local_agent_id].i_battery   = moving_average(i_battery_buffer) - i_battery_bias;
    agentes_data[local_agent_id].i_converter = moving_average(i_converter_buffer) - i_converter_bias;
    agentes_data[local_agent_id].i_panel     = moving_average(i_panel_buffer) - i_panel_bias;
    xSemaphoreGive(dataMutex);
  }

  avg_index++;
  if (avg_index >= AVG_WINDOW) {
    avg_index = 0;
    avg_filled = true;
  }

  // Debug opcional
  /*
  Serial.print("[SENSORES] v_cell1: "); Serial.print(agentes_data[local_agent_id].v_cell1);
  Serial.print(" | v_cell2: "); Serial.print(agentes_data[local_agent_id].v_cell2);
  Serial.print(" | v_panel: "); Serial.print(agentes_data[local_agent_id].v_panel);
  Serial.print(" | i_panel: "); Serial.print(agentes_data[local_agent_id].i_panel);
  Serial.println();
  */
}
