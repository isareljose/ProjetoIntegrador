#include <Arduino.h>
#include "HX711.h"
#include "soc/rtc.h"

// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 16;
const int LOADCELL_SCK_PIN = 4;
const float PESO_MEDIO_TAMPINHA = 3.0; // Peso médio em gramas (ajustar conforme necessário)

HX711 scale;
float pesoAcumulado = 0;
int totalTampinhas = 0;
unsigned long ultimaDeteccao = 0;
bool balancaTarada = false;
const unsigned long TEMPO_TARA = 5000; // 5 segundos sem novas tampinhas para tarar

void setup() {
  Serial.begin(115200);
  Serial.println("HX711 Demo");

  Serial.println("Initializing the scale");
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(-274.72248); // Ajuste conforme calibração
  scale.tare(); // Zerando a balança
  Serial.println("Scale initialized");
}

void loop() {
  float pesoAtual = scale.get_units(5);
  unsigned long tempoAtual = millis();

  if (pesoAtual > PESO_MEDIO_TAMPINHA * 0.5) { // Considerar se o peso for significativo
    pesoAcumulado += pesoAtual;
    int tampinhasDetectadas = round(pesoAcumulado / PESO_MEDIO_TAMPINHA);
    if (tampinhasDetectadas > totalTampinhas) {
      int novasTampinhas = tampinhasDetectadas - totalTampinhas;
      totalTampinhas = tampinhasDetectadas;
      Serial.print("Novas tampinhas detectadas: ");
      Serial.println(novasTampinhas);
      ultimaDeteccao = tempoAtual;
      balancaTarada = false; // Resetar a flag para permitir nova tara quando necessário
    }
  }

  // Se passar TEMPO_TARA sem novas tampinhas, tarar a balança
  if ((tempoAtual - ultimaDeteccao > TEMPO_TARA) && !balancaTarada) {
    scale.tare();
    pesoAcumulado = 0;
    Serial.println("Balança tarada automaticamente");
    balancaTarada = true;
  }

  delay(100);
}
