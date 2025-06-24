#include <Arduino.h>
#include "HX711.h"
#include "soc/rtc.h"
#include "BluetoothSerial.h"

// Configuração do Bluetooth
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run make menuconfig to and enable it
#endif
BluetoothSerial SerialBT;

// Configuração da balança HX711
const int LOADCELL_DOUT_PIN = 16;
const int LOADCELL_SCK_PIN = 4;
const float PESO_MEDIO_TAMPINHA = 3.6; // Peso médio em gramas (ajustar conforme necessário)
byte data[20];  // 20 bytes de dados
byte data10[10];  // 10 bytes de dados (para indicar que já contou)
HX711 scale;
int a = 0;

bool tampinhaDetectada = false;

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_TC");
  Serial.println("HX711 Demo");
  Serial.println("O dispositivo já pode ser conectado");

  Serial.println("Inicializando a balança...");
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(-246.55); // Ajuste conforme calibração
  scale.tare(); // Zerando a balança
  Serial.println("Balança inicializada");

  memset(data, 0x01, 20);  // Preenche os 20 bytes com 0x01
  memset(data10, 0x02, 10); // Preenche os 10 bytes com 0x02
}

void loop() {
  float pesoAtual = scale.get_units(5);

  delay(500);
  if (pesoAtual > PESO_MEDIO_TAMPINHA * 0.5) { // Considerar se o peso for significativo
    int quantidadeTampinhas = round(pesoAtual / PESO_MEDIO_TAMPINHA);
    Serial.print("Quantidade de tampinhas detectadas: ");
    Serial.println(quantidadeTampinhas);

    a = 0;
    
    tampinhaDetectada = true;

    if (quantidadeTampinhas >= 1) {
      // Envia os 20 bytes via Bluetooth
      a = quantidadeTampinhas;
      Serial.println("Enviado 20 bytes via Bluetooth");

      //finalizar a contagem
      //delay(500); // Aguarda estabilização antes de enviar os 10 bytes
      //SerialBT.write(data10, 10); // Envia 10 bytes para indicar que contou
      //Serial.println("Enviado 10 bytes via Bluetooth");
    } 

  }
  else if (pesoAtual <= PESO_MEDIO_TAMPINHA * 0.5) {
    tampinhaDetectada = false; // Reseta a flag quando a balança está vazia
    a = 0;
  }
  //delay(3000); // Pequeno delay para evitar oscilações
  //scale.set_scale(-246.55);
  //scale.tare(); // Tarar após contagem
  scale.power_down();
  delay(1000);
  scale.power_up();

  //SerialBT.println(a);
  //a = 0;
  //delay (100);
}
