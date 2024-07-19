#include "BluetoothSerial.h"
#include "DHTesp.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run make menuconfig to and enable it
#endif

#define LED_BUILTIN 2

int pinDHT1 = 12; 
int posicion[4]; 
int operation = 0;
TempAndHumidity sensor; 
DHTesp dht1;
int maxTemp = 40; 
int maxHumedity = 100; 
const int bufferLength = 10;
char buffer[bufferLength];
BluetoothSerial SerialBT;
int rele = 13; 
int threshold = 40;
volatile bool tactilActivado = false;
volatile bool tactilSalir = false;

void tactilUser() {
  tactilActivado = true;
}

void salirUser() {
  tactilSalir = true;
}

void setup() {
    Serial.begin(9600);
    Serial.println("");
    SerialBT.begin("ESP32");
    dht1.setup(pinDHT1, DHTesp::DHT22);
    pinMode(rele,OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT); //para mostrar correcto fun del bt
    touchAttachInterrupt(T9, tactilUser, threshold);
    touchAttachInterrupt(T7, salirUser, threshold);
    //attachInterrupt(digitalPinToInterrupt(pinBoton),  BtnPresionado, RISING); antes usando btn
}

void loop() {
    if (tactilActivado == true) {
        tactilActivado=false;//lo devuelvo a falso, asi cuando salgo del while vuelve al control de temp y espera una nueva interrupcion
        // hace toda su vaina de esperar lecturas y usar el BT
        digitalWrite(LED_BUILTIN, HIGH);
        clearSerialBTBuffer();
        // ahora se realizan los análisis
        while (true){
            String mensaje = GetLine(); // al inicio del modo deberías mandar un string así '1' + '#' o '0' + '#'
              int input = String(mensaje).toInt();
              
              if(input == 0){
                TempAndHumidity data1 = dht1.getTempAndHumidity();
                enviarInfoCliente(data1); // te envío ambos valores separados por |
                mostrarInfoPC(data1); // se muestra por la consola de Arduino
              }
              else if(input == 2){
                break;
              }
              else{
                int nueva_temp = input / 10000;
                int nueva_hum = input % 10000;
                procesar(nueva_temp);
                Serial.println("verificando tratamiento esperado");
                for (int i = 0; i < 4; i++) {
                    Serial.println(posicion[i]);
                }
                Serial.println("fin verificación");              
                operation = posicion[0] * 10 + posicion[1];
                actualizarParametros(operation);

                procesar(nueva_hum);
                Serial.println("verificando tratamiento esperado");
                for (int i = 0; i < 4; i++) {
                    Serial.println(posicion[i]);
                }
                Serial.println("fin verificación");              
                operation = posicion[0] * 10 + posicion[1];
                actualizarParametros(operation);             
              }             
              if(tactilSalir==true){
                tactilSalir=false;
                break;
              }
        }
        digitalWrite(LED_BUILTIN, LOW);//apago el led al salir del bucle
    } else { // solo que monitoree la configuración actual
        controlarDeshidratador(); // ejecuto la rutina de control de motor/led porque no hay motor :(
    }
}

// descompongo el número de 4 dígitos en instrucciones entendibles
void procesar(int codigo) {
    byte contador = 3; // voy de derecha a izquierda
    while (codigo >= 10) {
        posicion[contador] = codigo % 10;
        codigo = codigo / 10; // ya debería ser entero pero por si acaso
        contador--;
    }
    posicion[0] = codigo; // realizo la asignación de mi último dígito
}

// si se manda la orden de actualizar valores se ejecuta este set
void actualizarParametros(int operation) {
    switch (operation) {
        case 10: // update temp
            maxTemp = posicion[2] * 10 + posicion[3];
            break;
        case 20: // update humedad
            maxHumedity = posicion[2] * 10 + posicion[3];
            break;
        default:
            Serial.println("no debería pasar esto");
            break;
    }
}

// led intermitente que simula motor
void controlarDeshidratador() {
    TempAndHumidity data1 = dht1.getTempAndHumidity();
    while ((maxTemp <= data1.temperature) || (maxHumedity <= data1.humidity)) {
        digitalWrite(rele,HIGH);
        if(tactilActivado){
          digitalWrite(rele,LOW);
          break;
        }
    }
}

void enviarInfoCliente(TempAndHumidity sensor) {
    // La forma de enviar la data será separada por ; de esta manera descompones el mensaje que te envía el ESP32
    sensor = dht1.getTempAndHumidity();
    String info = String(sensor.temperature) + ';' + String(sensor.humidity) + ";\n"; // Son las lecturas actuales del sensor
    const char *dataBuffer = info.c_str(); // Usamos c_str() para obtener un puntero const char* al contenido de info
    size_t bufferSize = strlen(dataBuffer);
    SerialBT.write((const uint8_t *)dataBuffer, bufferSize);
}

void mostrarInfoPC(TempAndHumidity sensor) {
    Serial.println("Límites actuales de temp y hum: " + String(maxTemp) + " y " + String(maxHumedity));
    Serial.println("Humedad actual: " + String(sensor.humidity));
    Serial.println("Temperatura actual: " + String(sensor.temperature));
}

// método que me garantiza que espera hasta que el usuario envíe un carácter de #
String GetLine() {
    String S = "";
    char c;
    while (true) {
        if (SerialBT.available()) {
            c = SerialBT.read();
            if (c == '#') {
                break;
            }
            S += c;
        }
        delay(20);
    }
    return S; // con break se sale del while y se envía el código sin el #
}

// método que limpia el buffer para que se procesen los datos de forma consistente sin comportamiento crazy
void clearSerialBTBuffer() {
    while (SerialBT.available()) {
        SerialBT.read();
    }
}
