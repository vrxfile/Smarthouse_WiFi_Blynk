#define BLYNK_PRINT Serial

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include <Servo.h>
#include <AM2320.h>
#include <BH1750FVI.h>

// Точка доступа Wi-Fi
char ssid[] = "IOTIK";
char pass[] = "Terminator812";

// Датчик освещенности
BH1750FVI bh1750;

// Датчик температуры/влажности воздуха
AM2320 am2320;

// АЦП на ADS1115
Adafruit_ADS1115 ads1115;

// Выход реле
#define RELAY_PIN 16

// Датчик звука
#define SOUND_PIN A0

// Сервомотор
#define SERVO_PWM 14
Servo servo_motor;

// Датчик расстояния
#define US1_trigPin 13
#define US1_echoPin 12
#define minimumRange 0
#define maximumRange 400

// Период для таймера обновления данных
#define UPDATE_TIMER 5000

// Таймер
BlynkTimer timer_update;

// Параметры IoT сервера
char auth[] = "e267e6d47fd54663b0cd9cc6623a780f";
IPAddress blynk_ip(139, 59, 206, 133);

void setup()
{
  // Инициализация последовательного порта
  Serial.begin(115200);
  delay(1024);

  // Инициализация Wi-Fi и поключение к серверу Blynk
  Blynk.begin(auth, ssid, pass, blynk_ip, 8442);
  Serial.println();

  // Инициализация выхода реле
  pinMode(RELAY_PIN, OUTPUT);

  // Инициализация выводов для работы с УЗ датчиком расстояния
  pinMode(US1_trigPin, OUTPUT);
  pinMode(US1_echoPin, INPUT);

  // Инициализация порта для управления сервомотором
  servo_motor.attach(SERVO_PWM);
  servo_motor.write(90);
  delay(1024);

  // Инициализация таймера
  timer_update.setInterval(UPDATE_TIMER, readSendData);
}

void loop()
{
  Blynk.run();
  timer_update.run();
}

// Считывание датчиков и отправка данных на сервер Blynk
void readSendData()
{
  Wire.begin(0, 2);         // Инициализация I2C на выводах 0, 2
  Wire.setClock(10000L);    // Снижение тактовой частоты для надежности
  bh1750.begin();           // Инициализация датчика BH1750
  bh1750.setMode(Continuously_High_Resolution_Mode);
  float light = bh1750.getAmbientLight();               // Считывание датчика света
  Serial.print("Light = ");
  Serial.println(light);
  Blynk.virtualWrite(V2, light); delay(25);             // Отправка данных на сервер Blynk

  Wire.begin(4, 5);         // Инициализация I2C на выводах 4, 5
  Wire.setClock(10000L);    // Снижение тактовой частоты для надежности
  am2320.begin();           // Инициализация датчика AM2320
  if (am2320.measure())
  {
    float air_temp = am2320.getTemperature();            // Считывание температуры воздуха
    float air_hum = am2320.getHumidity();                // Считывание влажности воздуха
    Serial.print("Air temperature = ");
    Serial.println(air_temp);
    Serial.print("Air humidity = ");
    Serial.println(air_hum);
    Blynk.virtualWrite(V0, air_temp); delay(25);        // Отправка данных на сервер Blynk
    Blynk.virtualWrite(V1, air_hum); delay(25);         // Отправка данных на сервер Blynk
  }
  else
  {
    float air_temp = -127.0;
    float air_hum = -127.0;
    Serial.println("Error reading AM2320 sensor!");
    Blynk.virtualWrite(V0, air_temp); delay(25);        // Отправка данных на сервер Blynk
    Blynk.virtualWrite(V1, air_hum); delay(25);         // Отправка данных на сервер Blynk
  }

  ads1115.setGain(GAIN_TWOTHIRDS);
  ads1115.begin();                                      // Инициализация АЦП
  float fire  = (float)ads1115.readADC_SingleEnded(0) / 32767.0 * 100.0;
  float gas   = (float)ads1115.readADC_SingleEnded(1) / 32767.0 * 100.0;
  float water = (float)ads1115.readADC_SingleEnded(2) / 32767.0 * 100.0;
  float soil  = (float)ads1115.readADC_SingleEnded(3) / 32767.0 * 100.0;
  Serial.print("Fire detect = ");
  Serial.println(fire);
  Serial.print("Gas detect = ");
  Serial.println(gas);
  Serial.print("Water level = ");
  Serial.println(water);
  Serial.print("Soil moisture = ");
  Serial.println(soil);
  Blynk.virtualWrite(V4, fire); delay(25);        // Отправка данных на сервер Blynk
  Blynk.virtualWrite(V5, gas); delay(25);         // Отправка данных на сервер Blynk
  Blynk.virtualWrite(V6, water); delay(25);       // Отправка данных на сервер Blynk
  Blynk.virtualWrite(V7, soil); delay(25);        // Отправка данных на сервер Blynk

  float dist = readUS_distance();                 // Считывание расстояния с УЗ датчика
  Serial.print("Distance = ");
  Serial.println(dist);
  Blynk.virtualWrite(V3, dist); delay(25);        // Отправка данных на сервер Blynk

  float sound = readSensorSOUND();                // Считывание интенсивности звука
  Serial.print("Sound = ");
  Serial.println(sound);
  Blynk.virtualWrite(V8, sound); delay(25);       // Отправка данных на сервер Blynk
}

// Управление реле с Blynk
BLYNK_WRITE(V100)
{
  // Получение управляющего сигнала с сервера
  int relay_ctl = param.asInt();
  Serial.print("Relay power: ");
  Serial.println(relay_ctl);

  // Управление реле (помпой)
  digitalWrite(RELAY_PIN, relay_ctl);
}

// Управление сервомотором с Blynk
BLYNK_WRITE(V101)
{
  // Получение управляющего сигнала с сервера
  int servo_ctl = constrain(param.asInt(), 90, 135);
  Serial.print("Servo angle: ");
  Serial.println(servo_ctl);

  // Управление сервомотором
  servo_motor.write(servo_ctl);
}

// Считывание УЗ датчика расстояния
float readUS_distance()
{
  float duration = 0;
  float distance = 0;
  digitalWrite(US1_trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(US1_trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(US1_trigPin, LOW);
  duration = pulseIn(US1_echoPin, HIGH, 50000);
  distance = duration / 58.2;
  if (distance >= maximumRange || distance <= minimumRange) {
    distance = -1;
  }
  return distance;
}

// Считывание датчика звука
float readSensorSOUND()
{
  int max_a = 0;
  for (int i = 0; i < 128; i ++)
  {
    int a = analogRead(SOUND_PIN);
    if (a > max_a)
    {
      max_a = a;
    }
  }
  float db =  20.0  * log10 (max_a  + 1.0);
  return db;
}

