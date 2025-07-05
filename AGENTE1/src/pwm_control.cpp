#include "pwm_control.h"

void setupPWM() {
    ledc_timer_config_t timerConfig = {
        .speed_mode       = PWM_MODE,
        .duty_resolution  = PWM_RESOLUTION,
        .timer_num        = PWM_TIMER,
        .freq_hz          = PWM_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timerConfig);

    ledc_channel_config_t channelConfig = {
        .gpio_num   = PWM_PIN,
        .speed_mode = PWM_MODE,
        .channel    = PWM_CHANNEL,
        .timer_sel  = PWM_TIMER,
        .duty       = 0,
        .hpoint     = 0
    };
    ledc_channel_config(&channelConfig);
}

void PWM_Control_Task(void *pvParameters) {
  float voltaje = 0.0;
  float corriente = 0.0;
  float error_v = 0.0;
  float integral_error_v = 0.0;
  float error_i = 0.0;
  float integral_error_i = 0.0;
  float error_soc = 0.0;
  float integral_error_soc = 0.0;
  bool control_enabled = true;

  // SOC inicial
  float v_celda = agentes_data[local_agent_id].v_cell1 * 0.0001875;
  float soc_init;
  if (v_celda <= 3.2) {
    soc_init = 0.2 * (v_celda - 3.0) / 0.2;
  } else if (v_celda <= 4.0) {
    soc_init = 0.2 + 0.7 * (v_celda - 3.2) / 0.8;
  } else {
    soc_init = 0.9 + 0.1 * (v_celda - 4.0) / 0.2;
  }
  soc_init = constrain(soc_init, 0.0, 1.0);
  agentes_data[local_agent_id].soc = soc_init * 10000;
  Serial.print("SOC inicial estimado: ");
  Serial.println(agentes_data[local_agent_id].soc / 100.0);

  unsigned long start_time = millis();
  unsigned long end_time = 0;

  while (true) {
    start_time = millis();

    leerSensores();

    // --- Sección crítica: solo para modificar/agregar datos compartidos ---
    float i_batt_A, soc_float;
    int16_t soc_local, i_converter_local, v_barra_local, v_cell1_local, i_battery_local;
    unsigned long now;
    {
      if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        i_batt_A = agentes_data[local_agent_id].i_battery * 0.0001875 * 10.0;
        soc_float = agentes_data[local_agent_id].soc + ((i_batt_A * 0.05) / (5000.0 / 1000.0 * 3600.0)) * 10000.0;
        agentes_data[local_agent_id].soc = constrain((int16_t)soc_float, 0, 10000);
        agentes_data[local_agent_id].last_update = millis();

        // Copia local de variables necesarias para el control
        soc_local = agentes_data[local_agent_id].soc;
        i_converter_local = agentes_data[local_agent_id].i_converter;
        v_barra_local = agentes_data[local_agent_id].v_barra;
        v_cell1_local = agentes_data[local_agent_id].v_cell1;
        i_battery_local = agentes_data[local_agent_id].i_battery;
        now = millis();
        xSemaphoreGive(dataMutex);
      }
    }

    // --- Calcular promedios de agentes activos ---
    int agentes_activos = 0;
    int32_t suma_soc = 0;
    int32_t suma_i_converter = 0;

    for (size_t i = 0; i < NUM_AGENTS; ++i) {
      uint8_t id = known_agents[i];
      if (now - agentes_data[id].last_update < 2000) {
        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
          suma_soc += agentes_data[id].soc;
          suma_i_converter += agentes_data[id].i_converter;
          agentes_activos++;
          xSemaphoreGive(dataMutex);
        }
      }
    }
    // Siempre incluye al agente local (ya tienes la copia local)
    suma_soc += soc_local;
    suma_i_converter += i_converter_local;
    agentes_activos++;

    int16_t soc_promedio = agentes_activos ? suma_soc / agentes_activos : soc_local;
    int16_t i_converter_promedio = agentes_activos ? suma_i_converter / agentes_activos : i_converter_local;
    int16_t delta_soc = soc_promedio - soc_local;

    // --- Cálculo del control y PWM fuera de la sección crítica ---
    voltaje = v_barra_local * 0.0001875 * 2;
    corriente = i_converter_local * 0.0001875 * 10.0;

    if (control_enabled) {
      error_v = V_REF - voltaje - (delta_soc * 0.0001);
      integral_error_v += error_v * 0.05;
      float i_ref = KP_1 * error_v + KI_1 * integral_error_v;

      error_i = i_ref - corriente;
      integral_error_i += error_i * 0.05;
      float control = KP_2 * error_i + KI_2 * integral_error_i;

      int duty = constrain((int)control, DUTY_MIN, DUTY_MAX);
      ledc_set_duty(LEDC_HIGH_SPEED_MODE, PWM_CHANNEL, duty);
      ledc_update_duty(LEDC_HIGH_SPEED_MODE, PWM_CHANNEL);

      if (abs(delta_soc) > 500) control_enabled = false;
    } else {
      error_soc = delta_soc;
      integral_error_soc += error_soc * 0.05;
      float control_soc = KP_3 * error_soc + KI_3 * integral_error_soc;
      int duty = constrain((int)control_soc, DUTY_MIN, DUTY_MAX);
      ledc_set_duty(LEDC_HIGH_SPEED_MODE, PWM_CHANNEL, duty);
      ledc_update_duty(LEDC_HIGH_SPEED_MODE, PWM_CHANNEL);
    }

    end_time = millis();
    int delta = 50 + start_time - end_time;
    vTaskDelay(pdMS_TO_TICKS(max(0, delta)));
  }
}