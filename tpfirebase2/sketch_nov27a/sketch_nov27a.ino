// G4 - NAIARA D. - SOL F. - IVAN M. -


#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <U8g2lib.h>
#include <time.h>


#define DHTPIN  23
#define TYPE    DHT11
#define B1      35   // SW1: sube ciclo / cambia pantalla
#define B2      34   // SW2: baja ciclo / cambia pantalla


#define WIFI_SSID  "MECA-IoT-V2"
#define WIFI_PASS  "IoT$2026"
#define DB_URL     "https://tp-firebase-7d74f-default-rtdb.firebaseio.com/"


DHT dht(DHTPIN, TYPE);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);


void Pant1(float temp, int umb);
void Pant2(int ciclo);
void guardarFirebase();
String getFechaHora();


float temp  = 0;
int   umb   = 28;   // VU: valor umbral (referencia)
int   ciclo = 30;   // ciclo de guardado en Firebase (minimo 30 seg)


const int P1   = 1;
const int P2   = 2;
const int E1   = 3;   // espera soltar ambos (P1 → P2)
const int E2   = 4;   // espera soltar ambos (P2 → P1)
const int SUM  = 5;   // espera soltar SW1 para sumar
const int REST = 6;   // espera soltar SW2 para restar
const int EMA  = 7;   // ejecuta suma
const int EME  = 8;   // ejecuta resta


int P = P1;
unsigned long T_FIREBASE = 0;


void setup() {
  Serial.begin(9600);

  pinMode(B1, INPUT);
  pinMode(B2, INPUT);
  dht.begin();
  u8g2.begin();

  // CONECTAR WIFI
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14_tr);
  u8g2.drawStr(1, 20, "Conectando WiFi");
  u8g2.sendBuffer();

  WiFi.mode(WIFI_STA);                 // modo cliente
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println(WiFi.status());     // 1 = no encuentra la red, 4/6 = no lo deja entrar
  }
  Serial.println("WiFi conectado");

  // SINCRONIZAR HORA (GMT-3 Argentina)
  configTime(-3 * 3600, 0, "pool.ntp.org");
  struct tm t;
  while (!getLocalTime(&t)) {          // espera hasta tener la hora
    Serial.println("Esperando hora...");
    delay(500);
  }
  Serial.println("Hora OK: " + getFechaHora());

  // PRIMER GUARDADO
  float lectura = dht.readTemperature();
  if (!isnan(lectura)) temp = lectura;
  guardarFirebase();
  T_FIREBASE = millis();
}


void loop() {

  int BOT1 = digitalRead(B1);
  int BOT2 = digitalRead(B2);

  float lectura = dht.readTemperature();
  if (!isnan(lectura)) temp = lectura;   // solo actualiza si la lectura es valida

  // GUARDADO PERIODICO EN FIREBASE
  if (millis() - T_FIREBASE >= (unsigned long)ciclo * 1000UL) { // si ya paso el tiempo del ciclo
    guardarFirebase();
    T_FIREBASE = millis();
  }

  switch (P) {

    case P1: {
      Pant1(temp, umb);
      if (BOT1 == LOW && BOT2 == LOW) { // AMBOS → CAMBIAR PANTALLA
        P = E1;
      }
    }
    break;

    case E1: { // ESPERA QUE SUELTEN AMBOS
      if (BOT1 == HIGH && BOT2 == HIGH) {
        u8g2.clearBuffer();
        P = P2;
      }
    }
    break;

    case P2: {
      Pant2(ciclo);
      if (BOT1 == LOW && BOT2 == LOW) {  // AMBOS → CAMBIAR PANTALLA
        P = E2;
      } else if (BOT1 == LOW && BOT2 == HIGH) { // SW1 → SUBE CICLO
        P = SUM;
      } else if (BOT1 == HIGH && BOT2 == LOW) { // SW2 → BAJA CICLO
        P = REST;
      }
    }
    break;

    case E2: { // ESPERA QUE SUELTEN AMBOS
      if (BOT1 == HIGH && BOT2 == HIGH) {
        u8g2.clearBuffer();
        P = P1;
      }
    }
    break;

    case SUM: {
      if (BOT1 == HIGH) { // SI SUELTO PASA A EMA
        P = EMA;
      }
    }
    break;

    case REST: {
      if (BOT2 == HIGH) { // SI SUELTO PASA A EME
        P = EME;
      }
    }
    break;

    case EMA: {
      ciclo = ciclo + 30; // SUMA 30 SEG AL CICLO
      P = P2;             // VUELVE A P2
    }
    break;

    case EME: {
      if (ciclo > 30) ciclo = ciclo - 30; // RESTA 30 SEG (MINIMO 30)
      P = P2;                             // VUELVE A P2
    }
    break;
  }

  delay(20);
}


// === FUNCIONES ===

void guardarFirebase() {
  WiFiClientSecure cliente;              // conexion segura (https)
  cliente.setInsecure();                 // no revisa el certificado del servidor
  HTTPClient http;

  String url = String(DB_URL) + "registros.json";   // carpeta donde se guarda
  http.begin(cliente, url);
  http.addHeader("Content-Type", "application/json");

  // arma el paquete: {"temperatura":24.0,"fecha_hora":"27/11/2026 08:35:00"}
  String json = "{\"temperatura\":" + String(temp, 1) +
                ",\"fecha_hora\":\"" + getFechaHora() + "\"}";

  int codigo = http.POST(json);          // manda el paquete a Firebase

  if (codigo == 200) {
    Serial.println("Guardado OK: " + getFechaHora());
  } else {
    Serial.println("Error: " + String(codigo) + " " + http.errorToString(codigo));
  }
  http.end();
}

String getFechaHora() {
  struct tm t; // guarda todas las variables de la hora juntas
  if (!getLocalTime(&t)) return "sin hora";
  char buf[25];
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &t);
  return String(buf);
}

void Pant1(float temp, int umb) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB12_tr);
  u8g2.drawStr(1, 20, "PANTALLA 1");
  u8g2.setFont(u8g2_font_7x14_tr);

  u8g2.setCursor(1, 42);
  u8g2.print("VA: ");
  u8g2.print(temp, 1);
  u8g2.print(" C");

  u8g2.setCursor(1, 58);
  u8g2.print("VU: ");
  u8g2.print(umb);
  u8g2.print(" C");
  u8g2.sendBuffer();
}

void Pant2(int ciclo) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB12_tr);
  u8g2.drawStr(1, 20, "PANTALLA 2");
  u8g2.setFont(u8g2_font_7x14_tr);

  u8g2.setCursor(1, 42);
  u8g2.print("Ciclo:");
  u8g2.setCursor(1, 58);
  u8g2.print(ciclo);
  u8g2.print(" seg");
  u8g2.sendBuffer();
}