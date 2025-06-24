
/*
  ===================================================================
  ==    PROJETO COLETOR INTELIGENTE DE TAMPINHAS - VERSÃO FINAL     ==
  ===================================================================
  Integração do Sensor IR, Célula de Carga (HX711) e controle de
  fluxo (máquina de estados) para ESP32.

  Lógica:
  1. AGUARDANDO: Espera comando 'c' via Serial (simulando o App).
  2. TARING: Zera a balança e o contador para um novo ciclo.
  3. CONTANDO:
     - Interrupção do IR conta tampinhas em segundo plano.
     - Loop principal verifica o peso e o compara com o máximo.
  4. CHEIO: Para a coleta, exibe os resultados e volta a aguardar.
*/

// --- BIBLIOTECAS ---
#include <Arduino.h>
#include "HX711.h"

// ===================================================================
// ---               CONFIGURAÇÕES DO USUÁRIO                    ---
// ===================================================================

// --- PINOS ---
// Pinos para a Célula de Carga HX711
const int PINO_HX711_DT = 16;  // Pino DT (consistent with your files)
const int PINO_HX711_SCK = 4;   // Pino SCK (consistent with your files)

// Pino para o Sensor Infravermelho (IR)
const int PINO_SENSOR_IR = 33; // Pino do seu código do sensor IR

// --- PARÂMETROS DA BALANÇA ---
// ###################################################################
// ##  IMPORTANTE: Insira aqui o valor que você obteve com o script ##
// ##  de calibração.                                               ##
// ###################################################################
const float FATOR_DE_CALIBRACAO = -275.315; // EXEMPLO! SUBSTITUA PELO SEU VALOR!

// Defina o peso máximo em gramas que o recipiente suporta
const float PESO_MAXIMO_GRAMAS = 4500.0; // Ex: Meio quilo

// --- PARÂMETROS DO SENSOR IR ---
// Tempo (em milissegundos) para evitar que a mesma tampinha seja contada várias vezes
const int DEBOUNCE_DELAY_MS = 300;


// --- VARIÁVEIS GLOBAIS ---
HX711 scale;

// Máquina de Estados
enum EstadoProjetor { AGUARDANDO, TARING, CONTANDO, CHEIO };
EstadoProjetor estadoAtual = AGUARDANDO;

// 'volatile' é essencial pois estas variáveis são modificadas por uma interrupção.
volatile int contadorTampinhas = 0;
volatile unsigned long ultimoTempoDeteccao = 0;

// Variáveis de estado
float pesoAtual = 0;
long ultimoReporte = 0;


// ===================================================================
// ---        FUNÇÃO DE INTERRUPÇÃO (ISR) PARA O SENSOR IR       ---
// ===================================================================
// Esta função é chamada AUTOMATICAMENTE pelo ESP32 sempre que uma
// tampinha passa pelo sensor IR.
void IRAM_ATTR contarTampinhaISR() {
  unsigned long agora = millis();
  if (agora - ultimoTempoDeteccao > DEBOUNCE_DELAY_MS) {
    contadorTampinhas++;
    ultimoTempoDeteccao = agora;
  }
}


// ===================================================================
// ---                         SETUP                               ---
// ===================================================================
// Roda uma vez quando o ESP32 liga.
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n==============================================");
  Serial.println("Coletor de Tampinhas - Sistema Iniciado");
  Serial.println("==============================================");

  // Inicializa a balança
  scale.begin(PINO_HX711_DT, PINO_HX711_SCK);
  Serial.println("HX711 inicializado.");
  
  // Define o fator de calibração obtido anteriormente
  scale.set_scale(FATOR_DE_CALIBRACAO);
  Serial.print("Fator de calibração definido para: ");
  Serial.println(FATOR_DE_CALIBRACAO);

  // Zera a balança na inicialização
  scale.tare();

  // Inicializa o sensor IR
  pinMode(PINO_SENSOR_IR, INPUT);
  // Anexa a interrupção ao pino
  attachInterrupt(digitalPinToInterrupt(PINO_SENSOR_IR), contarTampinhaISR, FALLING);
  Serial.println("Sensor IR com interrupção configurado.");
  
  Serial.println("\nSistema em modo AGUARDANDO.");
  Serial.println("Envie 'c' pela Serial (ou app) para começar uma nova coleta.");
}


// ===================================================================
// ---                          LOOP                               ---
// ===================================================================
// Roda continuamente. A máquina de estados decide o que fazer.
void loop() {
  switch (estadoAtual) {
    case AGUARDANDO:
      aguardarComando();
      break;

    case TARING:
      executarTara();
      break;

    case CONTANDO:
      verificarPeso();
      reportarStatus(); // Mostra o status atual periodicamente
      break;

    case CHEIO:
      finalizarColeta();
      break;
  }
}


// --- FUNÇÕES DA MÁQUINA DE ESTADOS ---

void aguardarComando() {
  // Lógica para iniciar o ciclo. Substitua por sua comunicação Bluetooth/WiFi se necessário.
  if (Serial.available() > 0) {
    char comando = Serial.read();
    if (comando == 'c') {
      Serial.println("\n[!] Comando 'começar' recebido. Transicionando para TARING.");
      estadoAtual = TARING;
    }
  }
}

void executarTara() {
  Serial.println("[!] Esvazie o recipiente. Zerando a balança em 3 segundos...");
  delay(3000);
  
  // Reseta as variáveis para o novo ciclo
  contadorTampinhas = 0;
  pesoAtual = 0;
  scale.tare(); // Ação mais importante deste estado!
  
  Serial.println("[!] Tara concluída. Balança zerada.");
  Serial.println("[!] Tran   sicionando para CONTANDO. Pode inserir as tampinhas.");
  estadoAtual = CONTANDO;
  ultimoReporte = millis(); // Reseta o timer do reporte
}

void verificarPeso() {
  // Verifica se a balança tem uma leitura pronta
  if (scale.is_ready()) {
    // Pega uma média de 5 leituras para estabilizar o valor
    pesoAtual = scale.get_units(5);
    
    // Garante que o peso não seja negativo por alguma flutuação
    if (pesoAtual < 0) {
      pesoAtual = 0;
    }

    // Verifica se atingiu o peso máximo
    if (pesoAtual >= PESO_MAXIMO_GRAMAS) {
      Serial.println("\n[!] PESO MÁXIMO ATINGIDO!");
      estadoAtual = CHEIO;
    }
  }
}

void reportarStatus() {
  // A cada 2 segundos, imprime o status atual no monitor serial.
  if (millis() - ultimoReporte > 2000) {
    Serial.print("Status -> Tampinhas: ");
    Serial.print(contadorTampinhas);
    Serial.print(" | Peso: ");
    Serial.print(pesoAtual, 1); // Imprime com 1 casa decimal
    Serial.println(" g");
    
    ultimoReporte = millis(); // Atualiza o tempo do último reporte
    // Aqui você pode enviar o status para o App
  }
}

void finalizarColeta() {
  Serial.println("\n---------------------------------");
  Serial.println("      COLETA FINALIZADA");
  Serial.println("---------------------------------");
  Serial.print("Contagem final de tampinhas: ");
  Serial.println(contadorTampinhas);
  Serial.print("Peso final registrado: ");
  Serial.print(pesoAtual, 1);
  Serial.println(" g");
  Serial.println("---------------------------------\n");

  // AQUI VOCÊ DEVE ENVIAR OS DADOS FINAIS PARA O APP

  Serial.println("Sistema voltando para o modo AGUA   RDANDO.");
  Serial.println("Envie 'c' para começar uma nova coleta.");
  estadoAtual = AGUARDANDO;
  delay(1000);
}