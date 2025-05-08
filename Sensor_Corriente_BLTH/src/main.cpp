#include <Arduino.h>
#include "BluetoothSerial.h"
#include <Adafruit_ADS1X15.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//Sensor de voltaje
Adafruit_ADS1115 ads;
int16_t adc0;
int16_t adc1;

BluetoothSerial SerialBT;


const int adcPin = 34;
const float vRef = 3.3;
const float sensorCero = 1.95;
const float sensibilidad = 0.103;
int d = 500;


void setup() {
  Serial.begin(115200);          // Serial USB
  Wire.begin(21, 22);
  Serial.println("Iniciando...");
  SerialBT.begin("ESP32_SENSOR"); // Nombre del dispositivo Bluetooth

  ads.begin();
  ads.setGain(GAIN_TWOTHIRDS);

  analogReadResolution(12);
  delay(d);
  int adcValue = analogRead(adcPin);
}

void loop() {
  //Lectura de Voltaje
  int adcValue = analogRead(adcPin);
  //Calculos de Corriente

  //Calculos de Entrada
  adc0 = ads.readADC_SingleEnded(0);
  adc1 = ads.readADC_SingleEnded(1);
  float adc0volt = ads.computeVolts(adc0);
  float adc1volt = ads.computeVolts(adc1);

  //float corriente = (adc1volt-(adc0volt/sensorCero))/sensibilidad;
  float corriente = (adc1volt-(2.5))/sensibilidad;





//Printear

  String mensaje = "N:"+String(adc0,3)+"  V:"+String(adc0volt,5)+
  "     N2:"+String(adc1,3)+"  V:"+String(adc1volt,5)+ "  I:"+String(corriente,4);

  //String mensaje = "Referencia: " + String(referencia_media, 3) + " V   " +//"["+ String(resultado,1)+"]   "+
  //                 "Voltage: " + String(voltage, 3) + " V   " +
  //                 "Corriente: " + String(corriente, 3) + " A";

  Serial.println(mensaje);   // Por USB
  SerialBT.println(mensaje); // Por Bluetooth

  delay(d);
}






