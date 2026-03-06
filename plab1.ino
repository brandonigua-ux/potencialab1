#include <BluetoothSerial.h>
#include <LiquidCrystal_I2C.h>

BluetoothSerial SerialBT;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ============================
// CONFIGURACIÓN DE PINES
// ============================
const int pinVolt = 34;
const int pinCorr = 35;
const int pinBoton = 25;

const int N = 200;
const float Vref = 3.3;
const int ADCmax = 4095;

// ============================
// FACTORES DE ESCALA
// ============================

// 120 Vrms reducidos a 3.0 Vrms
float factorVoltaje = 170/1.5;

// Corriente solo quitando offset (no en amperios aún)
float factorCorriente = 0.82;

// ============================
// VECTORES
// ============================
float v[N];
float i[N];


// =====================================================
// SETUP
// =====================================================
void setup() {

  Serial.begin(115200);
  SerialBT.begin("AnalizadorESP32");

  pinMode(pinBoton, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();
}


// =====================================================
// ESPERAR CRUCE POR CERO (señal de voltaje)
// =====================================================
void esperarCruceCero() {

  int mitadADC = 2048;  // Aproximadamente 1.65V

  int anterior = analogRead(pinVolt);

  while (true) {

    int actual = analogRead(pinVolt);

    if ((anterior < mitadADC) && (actual >= mitadADC)) {
      break;
    }

    anterior = actual;
  }
}


// =====================================================
// TOMA DE MUESTRAS + ESCALADO
// =====================================================
void tomarMuestras() {

  float offsetV = 0;
  float offsetI = 0;

  // Calcular offset automático
  for(int k=0; k<N; k++){
    offsetV += analogRead(pinVolt);
    offsetI += analogRead(pinCorr);
  }

  offsetV /= N;
  offsetI /= N;

  // Tomar muestras reales
  for(int k=0; k<N; k++){

    int adcV = analogRead(pinVolt);
    int adcI = analogRead(pinCorr);

    // Quitar offset y convertir a voltaje ADC
    float v_adc = (adcV - offsetV) * (Vref / ADCmax);
    float i_adc = (adcI - offsetI) * (Vref / ADCmax);

    // Escalado final
    v[k] = v_adc * (factorVoltaje);
    i[k] = i_adc * (1/factorCorriente);
    
  }
}


// =====================================================
// CÁLCULOS Y ENVÍO DE RESULTADOS
// =====================================================
void calcularYMostrar() {

  float sumaV2 = 0;
  float sumaI2 = 0;
  float sumaP  = 0;

  float vmax = -10000;
  float vmin =  10000;

  for(int k=0; k<N; k++){

    sumaV2 += v[k]*v[k];
    sumaI2 += i[k]*i[k];
    sumaP  += v[k]*i[k];

    if(v[k] > vmax) vmax = v[k];
    if(v[k] < vmin) vmin = v[k];
  }

  float Vrms = sqrt(sumaV2/N);
  float Irms = sqrt(sumaI2/N);
  float P = sumaP/N;
  float S = Vrms * Irms;

  float Q = 0;
  if(S > P) Q = sqrt((S*S)-(P*P));

  float FP = 0;
  if(S != 0) FP = P/S;

  float angulo = 0;
  if(FP <= 1 && FP >= -1) angulo = acos(FP) * 180.0 / PI;

  float Vpp = vmax - vmin;

  // ---------------- SERIAL ----------------
  Serial.println("----- RESULTADOS -----");
  Serial.println("Vpp: " + String(Vpp));
  Serial.println("Vrms: " + String(Vrms));
  Serial.println("Irms: " + String(Irms));
  Serial.println("P (Activa): " + String(P));
  Serial.println("S (Aparente): " + String(S));
  Serial.println("Q (Reactiva): " + String(Q));
  Serial.println("FP: " + String(FP));
  Serial.println("Angulo: " + String(angulo));
  Serial.println("FACTOR V: " + String(factorVoltaje));
  Serial.println("----------------------");

  // ---------------- BLUETOOTH ----------------
  SerialBT.println("----- RESULTADOS -----");
  SerialBT.println("Vpp: " + String(Vpp));
  SerialBT.println("Vrms: " + String(Vrms));
  SerialBT.println("Irms: " + String(Irms));
  SerialBT.println("P: " + String(P));
  SerialBT.println("S: " + String(S));
  SerialBT.println("Q: " + String(Q));
  SerialBT.println("FP: " + String(FP));
  SerialBT.println("Angulo: " + String(angulo));
  SerialBT.println("----------------------");

  // ---------------- LCD 16x2 ----------------
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("V:");
  lcd.print(Vrms,1);
  lcd.print(" I:");
  lcd.print(Irms,1);

  lcd.setCursor(0,1);
  lcd.print("FP:");
  lcd.print(FP,2);
  lcd.print(" P:");
  lcd.print(P,0);
}


// =====================================================
// IMPRIMIR ÚLTIMAS 200 MUESTRAS
// =====================================================
void imprimirMuestras() {

  Serial.println("---- MUESTRAS ----");
  SerialBT.println("---- MUESTRAS ----");

  for(int k=0; k<N; k++){
    String linea = String(v[k]) + "," + String(i[k]);
    Serial.println(linea);
    SerialBT.println(linea);
  }

  Serial.println("---- FIN ----");
  SerialBT.println("---- FIN ----");
}


// =====================================================
// LOOP PRINCIPAL
// =====================================================
void loop() {

  esperarCruceCero();
  tomarMuestras();
  calcularYMostrar();

  if(digitalRead(pinBoton)==LOW){
    imprimirMuestras();
    delay(10);
  }

  
}