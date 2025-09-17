/*
  Sistema de Riego Automático con ESP32-S3
  Proyecto para Algoritmos y Estructuras de Datos I
  Instituto Tecnológico de Costa Rica
  Autores: Josafat Solano, Mathias Calvo, Camila Navarro
  Fecha: Septiembre 2025

  Descripción:
  Controla el riego de tres zonas independientes usando sensores de humedad, bombas y consulta meteorológica.
  Permite modos de operación automáticos, manuales y demo, con configuración de horarios y frecuencia de riego.
*/
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

#define PIN_SENSOR_1 4        // Pin del sensor de humedad de la zona 1
#define PIN_SENSOR_2 5        // Pin del sensor de humedad de la zona 2
#define PIN_SENSOR_3 6        // Pin del sensor de humedad de la zona 3
#define PIN_BOMBA_1 42        // Pin de la bomba de la zona 1
#define PIN_BOMBA_2 41        // Pin de la bomba de la zona 2
#define PIN_BOMBA_3 40        // Pin de la bomba de la zona 3
#define HORA_RIEGO_1 7        // Hora de riego zona 1
#define MINUTO_RIEGO_1 0      // Minuto de riego zona 1
#define DURACION_RIEGO_1 900  // Duración de riego zona 1 (segundos)
#define FRECUENCIA_RIEGO_1 1  // Frecuencia de riego zona 1 (cada n días)
#define HORA_RIEGO_2 21       // Hora de riego zona 2
#define MINUTO_RIEGO_2 0      // Minuto de riego zona 2
#define DURACION_RIEGO_2 600  // Duración de riego zona 2 (segundos)
#define FRECUENCIA_RIEGO_2 3  // Frecuencia de riego zona 2 (cada n días)
#define HORA_RIEGO_3 0        // Hora de riego zona 3
#define MINUTO_RIEGO_3 0      // Minuto de riego zona 3
#define DURACION_RIEGO_3 0    // Duración de riego zona 3 (segundos)
#define FRECUENCIA_RIEGO_3 0  // Frecuencia de riego zona 3 (cada n días)
#define UMBRAL_HUMEDAD 2500   // Umbral de humedad para activar riego

const char* WIFI_SSID = "virus";
const char* WIFI_PASS = "hellokitty12";
const char* CLIMA_API_KEY = "d9fb19db942b4691bfa30104251409";
const char* LATITUD = "9.856640";
const char* LONGITUD = "-83.912596";

int probabilidadLluvia = 0;  // Variable para guardar el % de lluvia

bool modoDemo = false;
bool modoManual = false;
bool bombaManual[3] = {false, false, false};

unsigned long manualMillisOffset = 0;
int manualHour = 0;
int manualMinute = 0;
int manualSecond = 0;

class Zona {
  /*
    Clase Zone
    Representa una zona de riego con sensor y bomba.
    Permite configurar horarios, duración, frecuencia y umbral de humedad.
    Métodos:
      - begin(): Inicializa pines.
      - leerSensor(): Lee humedad.
      - setBomba(): Controla bomba.
      - estadoBomba(): Estado actual de la bomba.
  */
  public:
  int pinSensor;
  int pinBomba;
  int horaRiego;
  int minutoRiego;
  int duracionRiego;
  int cadaNDias;
  int umbralMax;
  bool bombaEncendida;
  unsigned long bombaDesdeMillis;
  long ultimoDiaRiego;

  Zona(int s, int p, int h, int m, int d, int n, int uMax) {
      pinSensor = s; pinBomba = p;
      horaRiego = h; minutoRiego = m; duracionRiego = d; cadaNDias = n;
      umbralMax = uMax;
      bombaEncendida = false; bombaDesdeMillis = 0; ultimoDiaRiego = -1;
    }

  void iniciar() {
      pinMode(pinBomba, OUTPUT);
      digitalWrite(pinBomba, LOW);
      // Inicializa la bomba apagada
    }

  int leerSensor() {
      // Lee el valor del sensor de humedad
      return analogRead(pinSensor);
    }

  void establecerBomba(bool encender) {
      // Controla el estado de la bomba y muestra mensaje
      if (encender && !bombaEncendida) {
        digitalWrite(pinBomba, HIGH);
        bombaEncendida = true;
        bombaDesdeMillis = millis();
        Serial.print("💧 BOMBA ENCENDIDA - Zona ");
      } else if (!encender && bombaEncendida) {
        digitalWrite(pinBomba, LOW);
        bombaEncendida = false;
        Serial.print("🚫 BOMBA APAGADA - Zona ");
      }
    }

  bool estadoBomba() {
      return bombaEncendida;
    }
};

Zona zona1(PIN_SENSOR_1, PIN_BOMBA_1, HORA_RIEGO_1, MINUTO_RIEGO_1, DURACION_RIEGO_1, FRECUENCIA_RIEGO_1, UMBRAL_HUMEDAD);
Zona zona2(PIN_SENSOR_2, PIN_BOMBA_2, HORA_RIEGO_2, MINUTO_RIEGO_2, DURACION_RIEGO_2, FRECUENCIA_RIEGO_2, UMBRAL_HUMEDAD);
Zona zona3(PIN_SENSOR_3, PIN_BOMBA_3, HORA_RIEGO_3, MINUTO_RIEGO_3, DURACION_RIEGO_3, FRECUENCIA_RIEGO_3, UMBRAL_HUMEDAD);
Zona* zonas[] = { &zona1, &zona2, &zona3 };

bool suspenderRiegoHoy = false;
int ultimoDiaClima = -1;
bool tiempoConfigurado = false;
unsigned long ultimoTiempoLoop = 0;

void actualizarHoraManual() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate >= 1000) {
    lastUpdate = millis();
    manualSecond++;
    if (manualSecond >= 60) {
      manualSecond = 0;
      manualMinute++;
      if (manualMinute >= 60) {
        manualMinute = 0;
        manualHour++;
        if (manualHour >= 24) {
          manualHour = 0;
        }
      }
    }
  }
}

void establecerHoraManual(int hora, int minuto, int segundo) {
  manualHour = hora;
  manualMinute = minuto;
  manualSecond = segundo;
  manualMillisOffset = millis();
  Serial.printf("⏰ Hora manual configurada: %02d:%02d:%02d\n", hora, minuto, segundo);
}

bool obtenerHora(struct tm* timeinfo) {
  if (tiempoConfigurado) {
    return getLocalTime(timeinfo, 1000);
  } else {
    actualizarHoraManual();
    timeinfo->tm_hour = manualHour;
    timeinfo->tm_min = manualMinute;
    timeinfo->tm_sec = manualSecond;
    return true;
  }
}

long diasDesdeEpoca() {
  static long manualDays = 0;
  static int lastManualHour = -1;
  
  if (manualHour == 0 && lastManualHour == 23) {
    manualDays++;
  }
  lastManualHour = manualHour;
  
  return manualDays;
}

void conectarWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando a WiFi");
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 10) {
    delay(500);
    Serial.print(".");
    intentos++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Error conectando a WiFi");
  }
}

void configurarTiempo() {
  if (!tiempoConfigurado) {
    Serial.println("⏰ Intentando configurar tiempo NTP...");
    configTime(-6 * 3600, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
    delay(2000);
    
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10000)) {
      Serial.println("✅ Tiempo NTP configurado");
      tiempoConfigurado = true;
    } else {
      Serial.println("❌ NTP no disponible. Usando hora manual.");
      Serial.println("📌 Use 'T HH:MM:SS' para configurar hora manual");
  establecerHoraManual(12, 0, 0);
    }
  }
}

void actualizarClima() {
  // Consulta el clima y determina si se suspende el riego por alta probabilidad de lluvia
  if (modoDemo || modoManual) {
    Serial.println("🎮 MODO ESPECIAL: Ignorando pronóstico de lluvia");
    suspenderRiegoHoy = false;
    return;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi no conectado para consultar clima");
    return;
  }
  
  String url = String("http://api.weatherapi.com/v1/forecast.json?key=") + CLIMA_API_KEY +
               "&q=" + LATITUD + "," + LONGITUD + "&days=1";
  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, payload);
    if (!err) {
      probabilidadLluvia = doc["forecast"]["forecastday"][0]["day"]["daily_chance_of_rain"];
      suspenderRiegoHoy = (probabilidadLluvia >= 70);
      Serial.print("🌦 Probabilidad de lluvia: ");
      Serial.print(probabilidadLluvia);
      Serial.println("%");
      if (suspenderRiegoHoy) Serial.println("⛔ Se suspende el riego por clima");
      else Serial.println("✅ Se permite riego según humedad/calendario");
    }
  }
  http.end();
}

void setup() {
  /*
    setup()
    Inicializa la comunicación serie, WiFi, tiempo y zonas de riego.
    Muestra comandos disponibles en el monitor serie.
  */
  Serial.begin(115200);
  delay(2000);
  Serial.println("🚀 Iniciando sistema de riego Hidrosmart...");
  Serial.println("📌 Comandos disponibles:");
  Serial.println("   'D' - Modo Demo (ignora lluvia, auto)");
  Serial.println("   'M' - Modo Manual (control manual)");
  Serial.println("   '1', '2', '3' - Forzar bombas (en modo M)");
  Serial.println("   'T HH:MM:SS' - Configurar hora manual");
  
  conectarWiFi();
  configurarTiempo();
  
  for (int i = 0; i < 3; i++) zonas[i]->iniciar();
  Serial.println("✅ Zonas inicializadas");
}

void loop() {
  /*
    loop()
    Ciclo principal del sistema:
    - Procesa comandos por serial.
    - Actualiza clima y estado de riego.
    - Gestiona el riego de cada zona según modo, humedad y horario.
  */
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();
    // Procesa los comandos recibidos por el monitor serie
    if (comando == "D" || comando == "d") {
      modoDemo = !modoDemo;
      if (modoDemo) modoManual = false;
      Serial.print("🎮 Modo Demo: ");
      Serial.println(modoDemo ? "ACTIVADO" : "DESACTIVADO");
      ultimoTiempoLoop = 0;
    }
    else if (comando == "M" || comando == "m") {
      modoManual = !modoManual;
      if (modoManual) modoDemo = false;
      Serial.print("🎮 Modo Manual: ");
      Serial.println(modoManual ? "ACTIVADO" : "DESACTIVADO");
      for (int i = 0; i < 3; i++) {
        bombaManual[i] = false;
  zonas[i]->establecerBomba(false);
      }
      ultimoTiempoLoop = 0;
    }
    else if (comando.startsWith("T ")) {
      int hora = comando.substring(2, 4).toInt();
      int minuto = comando.substring(5, 7).toInt();
      int segundo = comando.substring(8, 10).toInt();
  establecerHoraManual(hora, minuto, segundo);
      ultimoTiempoLoop = 0;
    }
    else if (modoManual && (comando == "1" || comando == "2" || comando == "3")) {
      int numZona = comando.toInt() - 1;
      bombaManual[numZona] = !bombaManual[numZona];
  zonas[numZona]->establecerBomba(bombaManual[numZona]);
      Serial.print("🎮 Bomba Zona ");
      Serial.print(numZona + 1);
      Serial.print(" forzada: ");
      Serial.println(bombaManual[numZona] ? "ENCENDIDA" : "APAGADA");
      Serial.println("------ Actualización Inmediata ------");
      struct tm timeinfo;
  if (obtenerHora(&timeinfo)) {
        Serial.printf("Hora actual: %02d:%02d:%02d\n", 
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      }
      for (int i = 0; i < 3; i++) {
        Zona* z = zonas[i];
        int lectura = z->leerSensor();
        bool necesitaRiego = (lectura > z->umbralMax);
        Serial.printf("Zona %d -> Humedad: %d | ", i + 1, lectura);
        Serial.printf("Necesita riego: %s | ", necesitaRiego ? "SI" : "NO");
        Serial.printf("Bomba: %s\n", z->estadoBomba() ? "ENCENDIDA" : "APAGADA");
      }
      Serial.println("-----------------------------------");
      ultimoTiempoLoop = 0;
    }
  }
  
  if (WiFi.status() != WL_CONNECTED) {
  conectarWiFi();
  }
  // Obtiene la hora actual y gestiona el riego
  struct tm timeinfo;
  if (obtenerHora(&timeinfo)) {
    long hoy = tiempoConfigurado ? (time(nullptr) / 86400L) : diasDesdeEpoca();
    // Actualiza el clima si es un nuevo día y no está en modo especial
    if (ultimoDiaClima != hoy && !modoDemo && !modoManual) {
      actualizarClima();
      ultimoDiaClima = hoy;
    }
    // Muestra el estado del sistema cada 3 segundos
    if (millis() - ultimoTiempoLoop > 3000) {
      ultimoTiempoLoop = millis();
      Serial.println("------ Estado del sistema ------");
      Serial.printf("Hora actual: %02d:%02d:%02d", 
                   timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      Serial.printf(" | Modo: %s", tiempoConfigurado ? "NTP" : "MANUAL");
      Serial.printf(" | Demo: %s", modoDemo ? "SI" : "NO");
      Serial.printf(" | Manual: %s\n", modoManual ? "SI" : "NO");
      Serial.printf("🌦 Probabilidad de lluvia: %d%%", probabilidadLluvia);
      Serial.printf(" | Suspender riego: %s\n", suspenderRiegoHoy ? "SI" : "NO");
      for (int i = 0; i < 3; i++) {
        Zona* z = zonas[i];
        int lectura = z->leerSensor();
        bool necesitaRiego = (lectura > z->umbralMax);
        Serial.printf("Zona %d -> Humedad: %d | ", i + 1, lectura);
        Serial.printf("Necesita riego: %s | ", necesitaRiego ? "SI" : "NO");
        Serial.printf("Bomba: %s\n", z->estadoBomba() ? "ENCENDIDA" : "APAGADA");
      }
      Serial.println("-------------------------------");
    }
    // Lógica principal de riego para cada zona
    for (int i = 0; i < 3; i++) {
      Zona* z = zonas[i];
      // Si está en modo manual, respeta el estado forzado de la bomba
      if (modoManual) {
        z->establecerBomba(bombaManual[i]);
        continue;
      }
      int lectura = z->leerSensor();
      bool necesitaRiego = (lectura > z->umbralMax);
      bool activarPorHorario = false;
      // Verifica si corresponde activar por horario y frecuencia
      if (z->duracionRiego > 0 && z->cadaNDias > 0) {
        if (timeinfo.tm_hour == z->horaRiego && timeinfo.tm_min == z->minutoRiego) {
          if (z->ultimoDiaRiego != hoy) {
            long diasDesde = hoy - z->ultimoDiaRiego;
            if (z->ultimoDiaRiego == -1 || diasDesde >= z->cadaNDias) {
              activarPorHorario = true;
              z->ultimoDiaRiego = hoy;
            }
          }
        }
      }
      // Si hay que suspender por clima, apaga la bomba
      if (suspenderRiegoHoy && !modoDemo && !modoManual) {
        z->establecerBomba(false);
      } else if (activarPorHorario) {
        z->establecerBomba(true);
        z->bombaDesdeMillis = millis();
      } 
      // Apaga la bomba si ya pasó la duración de riego programada
      else if (z->bombaEncendida && activarPorHorario && (millis() - z->bombaDesdeMillis >= (unsigned long)z->duracionRiego * 1000UL)) {
        z->establecerBomba(false);
      }
      // Si la humedad lo requiere, enciende la bomba
      else {
        if (necesitaRiego) {
          z->establecerBomba(true);
        } else {
          z->establecerBomba(false);
        }
      }
    }
  }
  delay(1000);
}