/*
  ===================================================================
  ==  PROJETO COLETOR DE TAMPINHAS - VERSÃO PARA TESTE VIA SERIAL  ==
  ===================================================================
  Lógica de sessão para múltiplos usuários.
  - Comunicação: Via Monitor Serial da IDE do Arduino.
  - Sensor IR: Responsável pela contagem de tampinhas da sessão.
  - Balança: Responsável por verificar se o recipiente está cheio.
*/

#include <Arduino.h>
#include "HX711.h"
// A biblioteca BluetoothSerial foi removida.

// --- CONFIGURAÇÕES DO USUÁRIO ---
const int PINO_HX711_DT = 16;
const int PINO_HX711_SCK = 4;
const int PINO_SENSOR_IR = 33;

const float FATOR_DE_CALIBRACAO = -471.497; // SUBSTITUA PELO SEU VALOR!
const float PESO_MAXIMO_GRAMAS = 500.0;
const int DEBOUNCE_DELAY_MS = 300;

// --- OBJETOS E VARIÁVEIS GLOBAIS ---
HX711 scale;
// O objeto BluetoothSerial foi removido.

enum EstadoProjetor { AGUARDANDO, INICIANDO_SESSAO, CONTANDO_SESSAO, CHEIO };
EstadoProjetor estadoAtual = AGUARDANDO;

volatile int contadorTampinhasSessao = 0;
volatile unsigned long ultimoTempoDeteccao = 0;

float pesoTotalAtual = 0;
float pesoInicialSessao = 0;

void IRAM_ATTR contarTampinhaISR() {
  unsigned long agora = millis();
  if (agora - ultimoTempoDeteccao > DEBOUNCE_DELAY_MS) {
    contadorTampinhasSessao++;
    ultimoTempoDeteccao = agora;
  }
}

void setup() {
  Serial.begin(115200); // Canal de comunicação principal para teste
  Serial.println("\n\n==============================================");
  Serial.println("Coletor de Tampinhas - MODO DE TESTE SERIAL");
  Serial.println("==============================================");

  scale.begin(PINO_HX711_DT, PINO_HX711_SCK);
  scale.set_scale(FATOR_DE_CALIBRACAO);

  Serial.println("Tarando a balança com o recipiente vazio...");
  scale.tare();
  Serial.println("Tara inicial concluída.");
  
  pinMode(PINO_SENSOR_IR, INPUT);
  attachInterrupt(digitalPinToInterrupt(PINO_SENSOR_IR), contarTampinhaISR, FALLING);
  Serial.println("Sensor IR configurado para contagem.");
  
  Serial.println("\nSistema pronto. Aguardando comando no Monitor Serial.");
  Serial.println("Envie 'c' para começar ou 'p' para parar uma sessão.");
}

void loop() {
  switch (estadoAtual) {
    case AGUARDANDO:
      aguardarComando();
      break;
    case INICIANDO_SESSAO:
      iniciarNovaSessao();
      break;
    case CONTANDO_SESSAO:
      processarContagem();
      break;
    case CHEIO:
      Serial.println("Recipiente cheio. Nenhuma nova sessão pode ser iniciada até ser esvaziado.");
      delay(5000);
      break;
  }
}

// Todas as funções de comunicação agora usam 'Serial'
void aguardarComando() {
  if (Serial.available() > 0) {
    char comando = Serial.read();
    if (comando == 'c' && estadoAtual != CHEIO) {
      estadoAtual = INICIANDO_SESSAO;
    }
  }
}

void iniciarNovaSessao() {
  Serial.println("\n>>> Nova sessão iniciada! Por favor, insira as tampinhas uma por vez.");
  
  contadorTampinhasSessao = 0;
  pesoInicialSessao = scale.get_units(10);
  
  estadoAtual = CONTANDO_SESSAO;
}

void processarContagem() {
  if (Serial.available() > 0) {
    if (Serial.read() == 'p') {
      finalizarSessao("Usuário finalizou.");
      return;
    }
  }

  static int ultimoValorContado = -1;
  if (contadorTampinhasSessao != ultimoValorContado) {
    ultimoValorContado = contadorTampinhasSessao;
    String statusMsg = "Contagem da Sessão: " + String(contadorTampinhasSessao) + " tampinhas.";
    Serial.println(statusMsg);
  }

  if (scale.is_ready()) {
    pesoTotalAtual = scale.get_units(5);
    if (pesoTotalAtual >= PESO_MAXIMO_GRAMAS) {
      finalizarSessao("Recipiente Cheio!");
      estadoAtual = CHEIO;
    }
  }
  
  delay(100);
}

void finalizarSessao(String motivo) {
  Serial.println("---------------------------------");
  Serial.println("SESSÃO FINALIZADA");
  Serial.println("Motivo: " + motivo);
  Serial.println("---------------------------------");
  Serial.print("Total de tampinhas na sua sessão: ");
  Serial.println(contadorTampinhasSessao);
  Serial.println("---------------------------------");
  
  if(estadoAtual != CHEIO) {
    estadoAtual = AGUARDANDO;
    Serial.println("\nSistema pronto para o próximo usuário.");
    Serial.println("Envie 'c' para começar ou 'p' para parar uma sessão.");
  }
}