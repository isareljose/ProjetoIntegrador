/*
  ===================================================================
  ==  PROJETO COLETOR DE TAMPINHAS - VERSÃO COM ESTEIRA (TB6600)   ==
  ==  Código Final v4: Arquitetura totalmente não-bloqueante.      ==
  ===================================================================
*/

// --- BIBLIOTECAS ---
#include <Arduino.h>
#include "HX711.h"
#include <AccelStepper.h>

// --- CONFIGURAÇÕES DO USUÁRIO ---
const int PINO_HX711_DT = 16;
const int PINO_HX711_SCK = 4;
const int PINO_SENSOR_IR = 33;
const int PINO_MOTOR_STEP = 25;
const int PINO_MOTOR_DIR = 27;
const int PINO_MOTOR_ENABLE = 14;

// --- PARÂMETROS DE MOVIMENTO E COLETA ---
const float FATOR_DE_CALIBRACAO = -275.316;
const float PESO_MAXIMO_GRAMAS = 4500.0;
const int DEBOUNCE_DELAY_MS = 200;
const int VELOCIDADE_ESTEIRA = 1000;
const int ACELERACAO_ESTEIRA = 300;

// --- OBJETOS GLOBAIS ---
HX711 scale;
AccelStepper esteira(AccelStepper::DRIVER, PINO_MOTOR_STEP, PINO_MOTOR_DIR);

// --- MÁQUINA DE ESTADOS (COM MAIS ESTADOS PARA NÃO BLOQUEAR) ---
enum EstadoProjetor { AGUARDANDO, PREPARANDO_TARA, TARING, CONTANDO, CHEIO, FINALIZANDO };
EstadoProjetor estadoAtual = AGUARDANDO;

// --- VARIÁVEIS GLOBAIS ---
volatile int contadorTampinhas = 0;
volatile unsigned long ultimoTempoDeteccao = 0;
float pesoAtual = 0;

// --- VARIÁVEIS DE CONTROLE DE TEMPO (NÃO-BLOQUEANTE) ---
unsigned long tempoInicioPausa = 0;
unsigned long ultimoTempoPeso = 0;
unsigned long ultimoReporte = 0;
const long INTERVALO_LEITURA_PESO = 250; // Ler o peso a cada 250ms
const long INTERVALO_REPORTE = 2000;      // Reportar status a cada 2000ms

// --- ROTINA DE INTERRUPÇÃO (ISR) ---
void IRAM_ATTR contarTampinhaISR() {
  unsigned long agora = millis();
  if (agora - ultimoTempoDeteccao > DEBOUNCE_DELAY_MS) {
    contadorTampinhas++;
    ultimoTempoDeteccao = agora;
  }
}

// ===================================================================
// ---           SETUP - CONFIGURAÇÃO INICIAL                      ---
// ===================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n\nColetor de Tampinhas - v4 Nao-Bloqueante - Sistema Iniciado");

  // --- SETUP DA ESTEIRA ---
  esteira.setEnablePin(PINO_MOTOR_ENABLE);
  esteira.setPinsInverted(true, false, true); // Direção invertida
  esteira.setMaxSpeed(1500);
  esteira.setAcceleration(ACELERACAO_ESTEIRA);
  esteira.setSpeed(0);
  esteira.disableOutputs();
  Serial.println("Esteira inicializada corretamente.");

  // --- SETUP DA BALANÇA E SENSOR IR ---
  scale.begin(PINO_HX711_DT, PINO_HX711_SCK);
  scale.set_scale(FATOR_DE_CALIBRACAO);
  pinMode(PINO_SENSOR_IR, INPUT);
  attachInterrupt(digitalPinToInterrupt(PINO_SENSOR_IR), contarTampinhaISR, FALLING);
  
  Serial.println("\nSistema pronto. Em modo AGUARDANDO.");
}

// ===================================================================
// ---           LOOP PRINCIPAL                                    ---
// ===================================================================
void loop() {
  // A MÁQUINA DE ESTADOS AGORA CONTROLA TUDO
  switch (estadoAtual) {
    case AGUARDANDO:
      aguardarComando();
      break;

    case PREPARANDO_TARA:
      // Apenas marca o tempo e muda para o estado de espera
      Serial.println("[!] Esvazie o recipiente. Zerando a balanca em 3 segundos...");
      tempoInicioPausa = millis();
      estadoAtual = TARING;
      break;
      
    case TARING:
      // Fica esperando 3 segundos sem travar o programa
      if (millis() - tempoInicioPausa >= 3000) {
        executarTara();
      }
      break;

    case CONTANDO:
      // A TAREFA MAIS IMPORTANTE: Manter o motor girando de forma fluida.
      esteira.runSpeed();

      // Tarefas secundárias, executadas em intervalos, sem travar o motor.
      lerPesoPeriodicamente();
      reportarStatusPeriodicamente();
      break;

    case CHEIO:
      finalizarColeta();
      tempoInicioPausa = millis(); // Marca o tempo para a pausa final
      estadoAtual = FINALIZANDO;
      break;

    case FINALIZANDO:
      // Fica esperando 1 segundo antes de voltar ao início
      if (millis() - tempoInicioPausa >= 1000) {
        Serial.println("Sistema voltando para o modo AGUARDANDO.");
        estadoAtual = AGUARDANDO;
      }
      break;
  }
}

// ===================================================================
// ---           FUNÇÕES DE LÓGICA E ESTADO                        ---
// ===================================================================

void aguardarComando() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input == "c") {
      Serial.println("\n[!] Comando 'c' reconhecido.");
      estadoAtual = PREPARANDO_TARA; // Inicia o processo de tara não-bloqueante
    }
  }
}

void executarTara() {
  contadorTampinhas = 0;
  pesoAtual = 0;
  scale.tare();
  Serial.println("[!] Tara concluida. Balanca zerada.");
  Serial.println("[!] LIGANDO A ESTEIRA. Pode inserir as tampinhas.");
  esteira.enableOutputs();
  esteira.setSpeed(VELOCIDADE_ESTEIRA);
  estadoAtual = CONTANDO;
}

void lerPesoPeriodicamente() {
  if (millis() - ultimoTempoPeso >= INTERVALO_LEITURA_PESO) {
    ultimoTempoPeso = millis(); // Reseta o cronômetro da pesagem
    if (scale.is_ready()) {
      pesoAtual = scale.get_units(2); // Esta é a única operação bloqueante, mas agora só ocorre a cada 250ms.
      if (pesoAtual < 0) pesoAtual = 0;
      if (pesoAtual >= PESO_MAXIMO_GRAMAS) {
        estadoAtual = CHEIO;
      }
    }
  }
}

void finalizarColeta() {
  Serial.println("\n[!] PESO MAXIMO ATINGIDO!");
  esteira.stop();
  esteira.disableOutputs();

  Serial.println("\n---------------------------------");
  Serial.println("      COLETA FINALIZADA");
  Serial.println("---------------------------------");
  Serial.print("Contagem final de tampinhas: ");
  Serial.println(contadorTampinhas);
  Serial.print("Peso final registrado: ");
  Serial.print(pesoAtual, 1);
  Serial.println(" g");
  Serial.println("---------------------------------\n");
}

void reportarStatusPeriodicamente() {
  if (millis() - ultimoReporte >= INTERVALO_REPORTE) {
    ultimoReporte = millis(); // Reseta o cronômetro do reporte
    Serial.print("Status -> Tampinhas: ");
    Serial.print(contadorTampinhas);
    Serial.print(" | Peso: ");
    Serial.print(pesoAtual, 1);
    Serial.println(" g");
  }
}