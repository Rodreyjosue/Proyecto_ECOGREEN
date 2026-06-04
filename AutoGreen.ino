// LIBRERIAS
#include <Wire.h>                // Comunicación I2C
#include <LiquidCrystal_I2C.h>   // LCD con módulo I2C
#include <DHT.h>                 // Sensor DHT22


// DEFINICION DE PINES
#define DHTPIN 2            // Pin de datos del DHT22
#define DHTTYPE DHT22       // Tipo de sensor
#define SENSOR_SUELO A0     // Pin analógico del sensor de suelo
#define LDR_PIN A1          // Pin analógico de la LDR
#define LED_LDR 6           // LED que se enciende por baja luz
#define RELE_VENTILADOR 7   // Relé del ventilador
#define RELE_BUZZER 8       // Relé del buzzer
#define RELE_BOMBA 9        // Relé de la bomba


// OBJETOS
DHT dht(DHTPIN, DHTTYPE);                // Crear objeto DHT
LiquidCrystal_I2C lcd(0x27, 16, 2);      // LCD I2C 16x2


// VARIABLES DE CONFIGURACION

// Umbral de temperatura baja
float tempFrio = 20.0;

// Umbral de temperatura alta
float tempCalor = 28.0;

// Si el valor del suelo supera este umbral, se considera seco
int sueloSeco = 700;

// Si el valor de la LDR es menor o igual a este número, se considera baja luz
int umbralBajaLuz = 65;

bool buzzerPorBajaLuz = true;
bool releActivoEnLOW = true;



// VARIABLES PARA LA LCD
// Tiempo durante el cual se mostrará una pantalla de evento
unsigned long duracionEvento = 2500;

// Variable para controlar hasta cuándo se muestra el evento
unsigned long mostrarEventoHasta = 0;


// ENUMERACION DE EVENTOS
enum EventoSistema {
  SIN_EVENTO,
  EVENTO_SUELO_SECO,
  EVENTO_TEMP_ALTA,
  EVENTO_TEMP_BAJA,
  EVENTO_BAJA_LUZ
};

// Variables para controlar eventos
EventoSistema eventoActual = SIN_EVENTO;
EventoSistema ultimoEventoMostrado = SIN_EVENTO;


// FUNCIONES AUXILIARES PARA RELÉS
// Función para encender un relé
void encenderRele(int pin) {
  if (releActivoEnLOW) {
    digitalWrite(pin, LOW);   // En módulos activos en LOW, LOW enciende
  } else {
    digitalWrite(pin, HIGH);  // Si no, HIGH enciende
  }
}

// Función para apagar un relé
void apagarRele(int pin) {
  if (releActivoEnLOW) {
    digitalWrite(pin, HIGH);  // En módulos activos en LOW, HIGH apaga
  } else {
    digitalWrite(pin, LOW);   // Si no, LOW apaga
  }
}


// FUNCION PARA MOSTRAR LA PANTALLA PRINCIPAL
void mostrarPantallaPrincipal(float temp, int suelo, int luz, const char* estadoGeneral) {
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temp, 1);   // Temperatura con 1 decimal
  lcd.print("C S:");
  lcd.print(suelo);
  lcd.print("   ");     // Espacios para limpiar sobrantes

  lcd.setCursor(0, 1);
  lcd.print("L:");
  lcd.print(luz);
  lcd.print(" ");
  lcd.print(estadoGeneral);
  lcd.print("       "); // Espacios para limpiar sobrantes
}



// FUNCION PARA MOSTRAR EVENTOS
void mostrarPantallaEvento(EventoSistema evento) {
  lcd.clear();

  switch (evento) {
    case EVENTO_SUELO_SECO:
      lcd.setCursor(0, 0);
      lcd.print("SUELO SECO");
      lcd.setCursor(0, 1);
      lcd.print("BOMBA ON");
      break;

    case EVENTO_TEMP_ALTA:
      lcd.setCursor(0, 0);
      lcd.print("TEMP ALTA");
      lcd.setCursor(0, 1);
      lcd.print("VENTILADOR ON");
      break;

    case EVENTO_TEMP_BAJA:
      lcd.setCursor(0, 0);
      lcd.print("TEMP BAJA");
      lcd.setCursor(0, 1);
      lcd.print("BUZZER ON");
      break;

    case EVENTO_BAJA_LUZ:
      lcd.setCursor(0, 0);
      lcd.print("BAJA LUZ");
      lcd.setCursor(0, 1);
      lcd.print("LED ENCENDIDO");
      break;

    default:
      break;
  }
}


// SETUP
void setup() {
  // Iniciar comunicación serial
  Serial.begin(9600);

  // Iniciar sensor DHT22
  dht.begin();

  // Iniciar LCD
  lcd.init();
  lcd.backlight();

  // Configurar pines como salida
  pinMode(LED_LDR, OUTPUT);
  pinMode(RELE_VENTILADOR, OUTPUT);
  pinMode(RELE_BUZZER, OUTPUT);
  pinMode(RELE_BOMBA, OUTPUT);

  // Apagar LED al inicio
  digitalWrite(LED_LDR, LOW);

  // Apagar todos los relés al inicio
  apagarRele(RELE_VENTILADOR);
  apagarRele(RELE_BUZZER);
  apagarRele(RELE_BOMBA);

  // Mensaje de bienvenida
  lcd.setCursor(0, 0);
  lcd.print("AutoGreen");
  lcd.setCursor(0, 1);
  lcd.print("Iniciando...");
  delay(2000);

  // Limpiar LCD
  lcd.clear();
}



// LOOP PRINCIPAL
void loop() {

  // LECTURA DE SENSORES
  float temp = dht.readTemperature();    // Leer temperatura
  float hum = dht.readHumidity();        // Leer humedad ambiente
  int suelo = analogRead(SENSOR_SUELO);  // Leer humedad del suelo
  int luz = analogRead(LDR_PIN);         // Leer luz

  // Validar lectura del DHT22
  if (isnan(temp) || isnan(hum)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ERROR DHT22");
    lcd.setCursor(0, 1);
    lcd.print("Revise sensor");

    Serial.println("Error leyendo el DHT22");
    delay(1000);
    return;
  }

  // VARIABLES DE ESTADO
  bool ventiladorON = false;
  bool buzzerON = false;
  bool bombaON = false;
  bool ledON = false;


  // LOGICA DE LA BOMBA=
  if (suelo > sueloSeco) {
    encenderRele(RELE_BOMBA);
    bombaON = true;
  } else {
    apagarRele(RELE_BOMBA);
    bombaON = false;
  }


  // LOGICA DE TEMPERATURA
  if (temp < tempFrio) {
    // Hace frío → buzzer ON, ventilador OFF
    apagarRele(RELE_VENTILADOR);
    encenderRele(RELE_BUZZER);

    ventiladorON = false;
    buzzerON = true;
  }
  else if (temp >= tempCalor) {
    // Hace calor → ventilador ON, buzzer OFF
    encenderRele(RELE_VENTILADOR);
    apagarRele(RELE_BUZZER);

    ventiladorON = true;
    buzzerON = false;
  }
  else {
    // Temperatura normal → ambos apagados
    apagarRele(RELE_VENTILADOR);
    apagarRele(RELE_BUZZER);

    ventiladorON = false;
    buzzerON = false;
  }


  // LOGICA DE LUZ
  if (luz <= umbralBajaLuz) {
    digitalWrite(LED_LDR, HIGH);
    ledON = true;

    // Si se desea alerta también por baja luz
    if (buzzerPorBajaLuz) {
      encenderRele(RELE_BUZZER);
      buzzerON = true;
    }
  } else {
    digitalWrite(LED_LDR, LOW);
    ledON = false;
  }


  // DETERMINAR EVENTO CON PRIORIDAD
  eventoActual = SIN_EVENTO;

  if (bombaON) {
    eventoActual = EVENTO_SUELO_SECO;
  }
  else if (ventiladorON) {
    eventoActual = EVENTO_TEMP_ALTA;
  }
  else if (temp < tempFrio) {
    eventoActual = EVENTO_TEMP_BAJA;
  }
  else if (ledON) {
    eventoActual = EVENTO_BAJA_LUZ;
  }


  // ESTADO GENERAL
  const char* estadoGeneral = "NORMAL";

  if (bombaON) {
    estadoGeneral = "RIEGO";
  }
  else if (ventiladorON) {
    estadoGeneral = "CALOR";
  }
  else if (temp < tempFrio) {
    estadoGeneral = "FRIO";
  }
  else if (ledON) {
    estadoGeneral = "B.LUZ";
  }


  // MANEJO DE PANTALLA LCD
  unsigned long tiempoActual = millis();

  // Mostrar evento si es nuevo y no hay otro mostrándose
  if (eventoActual != SIN_EVENTO &&
      eventoActual != ultimoEventoMostrado &&
      tiempoActual >= mostrarEventoHasta) {

    mostrarPantallaEvento(eventoActual);
    ultimoEventoMostrado = eventoActual;
    mostrarEventoHasta = tiempoActual + duracionEvento;
  }
  // Si todavía sigue el tiempo del evento, no hacer nada
  else if (tiempoActual < mostrarEventoHasta) {
    // Se mantiene la pantalla del evento
  }
  // Si no hay evento, mostrar principal
  else {
    mostrarPantallaPrincipal(temp, suelo, luz, estadoGeneral);

    // Si ya no hay evento, se libera para uno nuevo
    if (eventoActual == SIN_EVENTO) {
      ultimoEventoMostrado = SIN_EVENTO;
    }
  }


  // MONITOR SERIAL
  Serial.print("Temp: ");
  Serial.print(temp);
  Serial.print(" C | Humedad: ");
  Serial.print(hum);
  Serial.print("% | Suelo: ");
  Serial.print(suelo);
  Serial.print(" | Luz: ");
  Serial.print(luz);
  Serial.print(" | Vent: ");
  Serial.print(ventiladorON ? "ON" : "OFF");
  Serial.print(" | Buzzer: ");
  Serial.print(buzzerON ? "ON" : "OFF");
  Serial.print(" | Bomba: ");
  Serial.print(bombaON ? "ON" : "OFF");
  Serial.print(" | LED: ");
  Serial.print(ledON ? "ON" : "OFF");
  Serial.print(" | Estado: ");
  Serial.println(estadoGeneral);

  delay(300);
}