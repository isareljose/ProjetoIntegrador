/*
  ===================================================================
  ==  PROJETO COLETOR DE TAMPINHAS - VERSÃO COM ESTEIRA (TB6600)   ==
  ===================================================================
  Código ajustado para incluir o pino Enable, uma boa prática ao
  usar drivers como o TB6600.
*/

// --- BIBLIOTECAS ---
#include <Arduino.h>
#include "HX711.h"
#include <AccelStepper.h>

// ===================================================================
// ---               CONFIGURAÇÕES DO USUÁRIO                    ---
// ===================================================================

// --- PINOS ---
const int PINO_HX711_DT = 16;
const int PINO_HX711_SCK = 4;
const int PINO_SENSOR_IR = 33;

// --- PINOS DO MOTOR DA ESTEIRA ---
const int PINO_MOTOR_STEP = 26;
const int PINO_MOTOR_DIR = 27;
const int PINO_MOTOR_ENABLE = 14; // <-- NOVO: Pino para habilitar/desabilitar o driver

// --- PARÂMETROS ---
const float FATOR_DE_CALIBRACAO = -275.316;
const float PESO_MAXIMO_GRAMAS = 4500.0;
const int DEBOUNCE_DELAY_MS = 300;
const int VELOCIDADE_ESTEIRA = 400; // Ajuste para a velocidade desejada (passos/segundo)

// --- OBJETOS GLOBAIS ---
HX711 scale;
AccelStepper esteira(AccelStepper::DRIVER, PINO_MOTOR_STEP, PINO_MOTOR_DIR);

enum EstadoProjetor { AGUARDANDO, TARING, CONTANDO, CHEIO };
EstadoProjetor estadoAtual = AGUARDANDO;

volatile int contadorTampinhas = 0;
volatile unsigned long ultimoTempoDeteccao = 0;

float pesoAtual = 0;
long ultimoReporte = 0;

void IRAM_ATTR contarTampinhaISR() {
  unsigned long agora = millis();
  if (agora - ultimoTempoDeteccao > DEBOUNCE_DELAY_MS) {
    contadorTampinhas++;
    ultimoTempoDeteccao = agora;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nColetor de Tampinhas com Esteira (Driver TB6600) - Sistema Iniciado");

  // --- SETUP DA ESTEIRA ---
  // <-- MUDANÇA: Adiciona o controle do pino Enable -->
  pinMode(PINO_MOTOR_ENABLE, OUTPUT);
  digitalWrite(PINO_MOTOR_ENABLE, HIGH); // Começa com o motor desabilitado

  esteira.setEnablePin(PINO_MOTOR_ENABLE);
  esteira.setPinsInverted(false, false, true); // Inverte a lógica do pino Enable (HIGH = desabilitado)
  esteira.setMaxSpeed(1000);
  esteira.setAcceleration(500);
  esteira.setSpeed(0);
  Serial.println("Esteira inicializada.");

  // ... resto do setup como antes ...
  scale.begin(PINO_HX711_DT, PINO_HX711_SCK);
  scale.set_scale(FATOR_DE_CALIBRACAO);
  scale.tare();
  pinMode(PINO_SENSOR_IR, INPUT);
  attachInterrupt(digitalPinToInterrupt(PINO_SENSOR_IR), contarTampinhaISR, FALLING);
  
  Serial.println("\nSistema em modo AGUARDANDO.");
}

void loop() {
  // O loop continua o mesmo, a biblioteca AccelStepper gerencia o pino Enable
  switch (estadoAtual) {
    case AGUARDANDO:
      // A esteira está parada e desenergizada
      break;
    case TARING:
      executarTara();
      break;
    case CONTANDO:
      esteira.runSpeed(); // Mantém a esteira em movimento
      verificarPeso();
      reportarStatus();
      break;
    case CHEIO:
      finalizarColeta();
      break;
  }
}

void executarTara() {
  Serial.println("[!] Esvazie o recipiente. Zerando a balança em 3 segundos...");
  delay(3000);
  
  contadorTampinhas = 0;
  pesoAtual = 0;
  scale.tare();
  
  Serial.println("[!] Tara concluída. Balança zerada.");
  Serial.println("[!] Transicionando para CONTANDO. Pode inserir as tampinhas.");
  
  // Liga e habilita a esteira
  esteira.enableOutputs(); // Habilita o driver
  esteira.setSpeed(VELOCIDADE_ESTEIRA);
  
  estadoAtual = CONTANDO;
  ultimoReporte = millis();
}

void verificarPeso() {
  if (scale.is_ready()) {
    pesoAtual = scale.get_units(5);
    if (pesoAtual < 0) pesoAtual = 0;
    if (pesoAtual >= PESO_MAXIMO_GRAMAS) {
      Serial.println("\n[!] PESO MÁXIMO ATINGIDO!");
      
      esteira.stop(); // Para a esteira
      esteira.disableOutputs(); // Desenergiza o motor
      
      estadoAtual = CHEIO;
    }
  }
}

void finalizarColeta() {
  // A esteira já foi parada e desabilitada
  Serial.println("\n---------------------------------");
  Serial.println("      COLETA FINALIZADA");
  // ... resto da função como antes ...
  Serial.println("---------------------------------");
  Serial.print("Contagem final de tampinhas: ");
  Serial.println(contadorTampinhas);
  Serial.print("Peso final registrado: ");
  Serial.print(pesoAtual, 1);
  Serial.println(" g");
  Serial.println("---------------------------------\n");

  Serial.println("Sistema voltando para o modo AGUARDANDO.");
  estadoAtual = AGUARDANDO;
  delay(1000);
}

// A função reportarStatus continua a mesma
void reportarStatus() {
  if (millis() - ultimoReporte > 2000) {
    Serial.print("Status -> Tampinhas: ");
    Serial.print(contadorTampinhas);
    Serial.print(" | Peso: ");
    Serial.print(pesoAtual, 1);
    Serial.println(" g");
    
    ultimoReporte = millis();
  }
}