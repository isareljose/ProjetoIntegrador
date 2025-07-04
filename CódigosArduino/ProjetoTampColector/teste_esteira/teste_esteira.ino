/*
  ======================================================
  ==     TESTE SIMPLES DE DIAGNÓSTICO - APENAS O MOTOR    ==
  ======================================================
  Este código serve apenas para verificar se o motor de passo,
  o driver TB6600 e a fiação estão funcionando.
  Ele tentará mover o motor 10 voltas para um lado e
  depois 10 voltas para o outro, continuamente.
*/

#include <Arduino.h>
#include <AccelStepper.h>

// --- PINOS DO MOTOR DA ESTEIRA ---
const int PINO_MOTOR_STEP = 26;
const int PINO_MOTOR_DIR = 27;
const int PINO_MOTOR_ENABLE = 14;

// Cria o objeto para o motor da esteira
AccelStepper esteira(AccelStepper::DRIVER, PINO_MOTOR_STEP, PINO_MOTOR_DIR);

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- INICIANDO TESTE DO MOTOR ---");

  // --- SETUP DA ESTEIRA ---
  pinMode(PINO_MOTOR_ENABLE, OUTPUT);
  digitalWrite(PINO_MOTOR_ENABLE, HIGH); // Começa com o motor desabilitado

  esteira.setEnablePin(PINO_MOTOR_ENABLE);
  esteira.setPinsInverted(false, false, true); // Inverte a lógica do pino Enable (HIGH = desabilitado)
  esteira.setMaxSpeed(1000); // Velocidade máxima
  esteira.setAcceleration(500); // Aceleração
  
  Serial.println("Habilitando o motor...");
  esteira.enableOutputs(); // Habilita o driver
  
  // Define uma posição alvo inicial
  esteira.moveTo(16000); // 1600 (1/8 de passo) * 10 voltas
}

void loop() {
  // Se o motor chegou ao seu destino...
  if (esteira.distanceToGo() == 0) {
    // ...espera 1 segundo e define um novo destino (a posição oposta)
    delay(1000);
    esteira.moveTo(-esteira.currentPosition());
    Serial.print("Motor chegou ao destino. Nova posição alvo: ");
    Serial.println(esteira.targetPosition());
  }

  // Esta função deve ser chamada o mais rápido possível no loop.
  // Ela calcula e envia os pulsos de passo necessários.
  esteira.run();
}
