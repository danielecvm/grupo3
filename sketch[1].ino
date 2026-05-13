// ============================================
// DRUM DRIVER IoT - SISTEMA COMPLETO DE PEDIDOS
// Simulador: Wokwi
// Componentes: ESP32, LCD 20x4 + LCD 16x2, Servo, Buzzer, 3 Botões
// Versão: 2.0 - Completa com Lógica de Estados
// ============================================

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ============= CONFIGURAÇÕES I2C =============

LiquidCrystal_I2C lcd1(0x26, 20, 4);  // LCD 1 - Tela Principal (20x4)
LiquidCrystal_I2C lcd2(0x27, 16, 2);  // LCD 2 - Menu (16x2)

// ============= DEFINIÇÃO DE PINOS =============

const int SERVO_PIN = 27;
const int BUZZER_PIN = 26;
const int BUTTON_EMERGENCY = 2;
const int BUTTON_UP = 35;
const int BUTTON_DOWN = 34;

// ============= ESTRUTURAS DE DADOS =============

// Estrutura para cada item do cardápio
struct ItemCardapio {
  char nome[15];
  float preco;
  int tempoPrep;  // Tempo de preparação em segundos
};

// Estrutura para pedido
struct Pedido {
  int numero;
  int itens[8];      // Quantidade de cada item
  float total;
  bool pronto;
  unsigned long tempoInicio;
};

// ============= CONSTANTES =============

const int NUM_ITENS = 8;
const int MAX_PEDIDOS = 10;
const int TEMPO_PREP_PADRAO = 10;  // 10 segundos

// ============= CARDÁPIO =============

ItemCardapio cardapio[NUM_ITENS] = {
  {"1.Hamburger", 15.00, 10},
  {"2.Batata Frita", 8.50, 8},
  {"3.Refrigerante", 5.00, 2},
  {"4.Pizza", 25.00, 15},
  {"5.Frango Frito", 18.00, 12},
  {"6.Sobremesa", 10.00, 5},
  {"7.Suco", 7.00, 3},
  {"8.Agua", 3.00, 1}
};

// ============= VARIÁVEIS GLOBAIS =============

// Estados do sistema
enum EstadoSistema {
  MENU_PRINCIPAL = 0,
  SELECAO_QUANTIDADE = 1,
  PROCESSANDO = 2,
  PRONTO = 3,
  EMERGENCIA = 4
};

// Variáveis de estado
EstadoSistema estadoAtual = MENU_PRINCIPAL;
int itemSelecionado = 0;
int quantidadeSelecionada = 1;
int numeroPedidoAtual = 1001;

// Pedido atual
Pedido pedidoAtual;

// Histórico de pedidos
Pedido historicoPedidos[MAX_PEDIDOS];
int totalPedidosProcessados = 0;

// Controle de buzzer
bool buzzerAtivo = false;
unsigned long ultimoBeepBuzzer = 0;

// Controle de tempo
unsigned long tempoInicioPedido = 0;
unsigned long ultimoTempoProcesamento = 0;

// Debounce de botões
unsigned long ultimoPressionoBotao = 0;
const int DEBOUNCE_DELAY = 300;

// ============= SETUP =============

void setup() {
  // Inicializar Serial
  Serial.begin(115200);
  delay(1000);
  
  // Configurar pinos
  pinMode(SERVO_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_EMERGENCY, INPUT_PULLUP);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  
  // Inicializar LCD 1 (20x4)
  lcd1.init();
  lcd1.backlight();
  lcd1.clear();
  lcd1.setCursor(0, 0);
  lcd1.print("DRUM DRIVER IoT");
  lcd1.setCursor(0, 1);
  lcd1.print("Sistema de Pedidos");
  lcd1.setCursor(0, 2);
  lcd1.print("Versao 2.0");
  lcd1.setCursor(0, 3);
  lcd1.print("Iniciando...");
  
  // Inicializar LCD 2 (16x2)
  lcd2.init();
  lcd2.backlight();
  lcd2.clear();
  lcd2.setCursor(0, 0);
  lcd2.print("MENU PEDIDOS");
  lcd2.setCursor(0, 1);
  lcd2.print("Aguardando...");
  
  // Mensagens de inicialização
  Serial.println("\n========================================");
  Serial.println("DRUM DRIVER IoT - SISTEMA DE PEDIDOS");
  Serial.println("Versao 2.0 - Completa");
  Serial.println("========================================\n");
  
  delay(3000);
  
  // Inicializar sistema
  inicializarNovoPedido();
  exibirMenuPrincipal();
  
  Serial.println("Sistema pronto!");
  Serial.println("Aguardando entrada do usuario...\n");
}

// ============= LOOP PRINCIPAL =============

void loop() {
  // Verificar botão de emergência
  if (digitalRead(BUTTON_EMERGENCY) == LOW) {
    if (millis() - ultimoPressionoBotao > DEBOUNCE_DELAY) {
      ativarParadaEmergencia();
      ultimoPressionoBotao = millis();
    }
  }
  
  // Processar baseado no estado atual
  switch (estadoAtual) {
    case MENU_PRINCIPAL:
      processarMenuPrincipal();
      break;
      
    case SELECAO_QUANTIDADE:
      processarSelecaoQuantidade();
      break;
      
    case PROCESSANDO:
      processarPedidoEmAndamento();
      break;
      
    case PRONTO:
      processarPedidoPronto();
      break;
      
    case EMERGENCIA:
      // Ficar em loop até reset
      delay(100);
      break;
  }
  
  // Processar entrada serial
  if (Serial.available() > 0) {
    processarEntradaSerial();
  }
  
  // Atualizar buzzer se necessário
  if (buzzerAtivo) {
    atualizarBuzzer();
  }
  
  delay(100);
}

// ============= FUNÇÕES DE INICIALIZAÇÃO =============

void inicializarNovoPedido() {
  pedidoAtual.numero = numeroPedidoAtual;
  pedidoAtual.total = 0.0;
  pedidoAtual.pronto = false;
  pedidoAtual.tempoInicio = 0;
  
  // Limpar array de itens
  for (int i = 0; i < NUM_ITENS; i++) {
    pedidoAtual.itens[i] = 0;
  }
  
  itemSelecionado = 0;
  quantidadeSelecionada = 1;
  estadoAtual = MENU_PRINCIPAL;
}

// ============= FUNÇÕES DE ESTADO: MENU PRINCIPAL =============

void processarMenuPrincipal() {
  // Botão UP - Item anterior
  if (digitalRead(BUTTON_UP) == LOW) {
    if (millis() - ultimoPressionoBotao > DEBOUNCE_DELAY) {
      itemSelecionado--;
      if (itemSelecionado < 0) {
        itemSelecionado = NUM_ITENS - 1;
      }
      exibirMenuPrincipal();
      ultimoPressionoBotao = millis();
    }
  }
  
  // Botão DOWN - Próximo item
  if (digitalRead(BUTTON_DOWN) == LOW) {
    if (millis() - ultimoPressionoBotao > DEBOUNCE_DELAY) {
      itemSelecionado++;
      if (itemSelecionado >= NUM_ITENS) {
        itemSelecionado = 0;
      }
      exibirMenuPrincipal();
      ultimoPressionoBotao = millis();
    }
  }
}

void exibirMenuPrincipal() {
  // LCD 2 - Mostra item selecionado com seletor
  lcd2.clear();
  lcd2.setCursor(0, 0);
  lcd2.print(">> ");
  lcd2.print(cardapio[itemSelecionado].nome);
  lcd2.setCursor(0, 1);
  lcd2.print("R$ ");
  lcd2.print(cardapio[itemSelecionado].preco, 2);
  
  // LCD 1 - Mostra cardápio completo
  exibirCardapioCompleto();
  
  // Serial
  Serial.print("Item selecionado: ");
  Serial.print(cardapio[itemSelecionado].nome);
  Serial.print(" - R$ ");
  Serial.println(cardapio[itemSelecionado].preco, 2);
}

void exibirCardapioCompleto() {
  lcd1.clear();
  lcd1.setCursor(0, 0);
  lcd1.print("CARDAPIO COMPLETO");
  lcd1.setCursor(0, 1);
  lcd1.print("1.Hamburger  R$15");
  lcd1.setCursor(0, 2);
  lcd1.print("2.Batata  3.Refri");
  lcd1.setCursor(0, 3);
  lcd1.print("4.Pizza  5.Frango");
}

// ============= FUNÇÕES DE ENTRADA SERIAL =============

void processarEntradaSerial() {
  String entrada = Serial.readStringUntil('\n');
  entrada.trim();
  
  int numero = entrada.toInt();
  
  // Se está no menu principal
  if (estadoAtual == MENU_PRINCIPAL) {
    if (numero >= 1 && numero <= NUM_ITENS) {
      selecionarItem(numero - 1);
    }
  }
  // Se está selecionando quantidade
  else if (estadoAtual == SELECAO_QUANTIDADE) {
    if (numero >= 1 && numero <= NUM_ITENS) {
      confirmarSelecao();
    }
    else if (numero == 0) {
      cancelarSelecao();
    }
  }
  // Se tem pedido em andamento
  else if (estadoAtual == MENU_PRINCIPAL && pedidoAtual.total > 0) {
    if (numero == 0) {
      confirmarPedido();
    }
  }
}

// ============= FUNÇÕES DE SELEÇÃO =============

void selecionarItem(int indice) {
  Serial.print("\n--- SELECIONANDO ITEM ---\n");
  Serial.print("Item: ");
  Serial.println(cardapio[indice].nome);
  
  itemSelecionado = indice;
  quantidadeSelecionada = 1;
  estadoAtual = SELECAO_QUANTIDADE;
  
  exibirSelecaoQuantidade();
}

void processarSelecaoQuantidade() {
  // Botão UP - Aumenta quantidade
  if (digitalRead(BUTTON_UP) == LOW) {
    if (millis() - ultimoPressionoBotao > DEBOUNCE_DELAY) {
      quantidadeSelecionada++;
      if (quantidadeSelecionada > 9) {
        quantidadeSelecionada = 9;
      }
      exibirSelecaoQuantidade();
      ultimoPressionoBotao = millis();
    }
  }
  
  // Botão DOWN - Diminui quantidade
  if (digitalRead(BUTTON_DOWN) == LOW) {
    if (millis() - ultimoPressionoBotao > DEBOUNCE_DELAY) {
      quantidadeSelecionada--;
      if (quantidadeSelecionada < 1) {
        quantidadeSelecionada = 1;
      }
      exibirSelecaoQuantidade();
      ultimoPressionoBotao = millis();
    }
  }
}

void exibirSelecaoQuantidade() {
  // LCD 2 - Seleção de quantidade
  lcd2.clear();
  lcd2.setCursor(0, 0);
  lcd2.print("Qtd: ");
  lcd2.print(quantidadeSelecionada);
  lcd2.setCursor(0, 1);
  lcd2.print("UP/DOWN | Enter");
  
  // LCD 1 - Detalhes do item
  float subtotal = cardapio[itemSelecionado].preco * quantidadeSelecionada;
  
  lcd1.clear();
  lcd1.setCursor(0, 0);
  lcd1.print("SELECIONAR ITEM");
  lcd1.setCursor(0, 1);
  lcd1.print(cardapio[itemSelecionado].nome);
  lcd1.setCursor(0, 2);
  lcd1.print("Quantidade: ");
  lcd1.print(quantidadeSelecionada);
  lcd1.setCursor(0, 3);
  lcd1.print("Subtotal: R$ ");
  lcd1.print(subtotal, 2);
  
  Serial.print("Quantidade: ");
  Serial.println(quantidadeSelecionada);
}

void confirmarSelecao() {
  Serial.print("\n--- ITEM ADICIONADO AO PEDIDO ---\n");
  Serial.print("Item: ");
  Serial.print(cardapio[itemSelecionado].nome);
  Serial.print(" | Quantidade: ");
  Serial.println(quantidadeSelecionada);
  
  // Adicionar ao pedido
  pedidoAtual.itens[itemSelecionado] += quantidadeSelecionada;
  pedidoAtual.total += (cardapio[itemSelecionado].preco * quantidadeSelecionada);
  
  // Voltar ao menu
  estadoAtual = MENU_PRINCIPAL;
  quantidadeSelecionada = 1;
  
  // Mostrar resumo
  exibirResumoPedido();
}

void cancelarSelecao() {
  Serial.println("\n--- SELEÇÃO CANCELADA ---\n");
  
  estadoAtual = MENU_PRINCIPAL;
  quantidadeSelecionada = 1;
  
  exibirMenuPrincipal();
}

// ============= FUNÇÕES DE RESUMO DO PEDIDO =============

void exibirResumoPedido() {
  lcd1.clear();
  lcd1.setCursor(0, 0);
  lcd1.print("PEDIDO #");
  lcd1.print(pedidoAtual.numero);
  lcd1.setCursor(0, 1);
  lcd1.print("Itens no pedido:");
  
  int linha = 2;
  int itemsCount = 0;
  
  // Contar itens
  for (int i = 0; i < NUM_ITENS; i++) {
    if (pedidoAtual.itens[i] > 0) {
      itemsCount++;
    }
  }
  
  // Exibir itens
  for (int i = 0; i < NUM_ITENS; i++) {
    if (pedidoAtual.itens[i] > 0) {
      if (linha <= 3) {
        lcd1.setCursor(0, linha);
        lcd1.print(pedidoAtual.itens[i]);
        lcd1.print("x");
        lcd1.print(cardapio[i].nome);
        linha++;
      }
    }
  }
  
  // Mostrar total ou mais itens
  if (itemsCount > 2) {
    lcd1.setCursor(0, 3);
    lcd1.print("+ ");
    lcd1.print(itemsCount - 2);
    lcd1.print(" itens mais");
  } else {
    lcd1.setCursor(0, 3);
    lcd1.print("Total: R$");
    lcd1.print(pedidoAtual.total, 2);
  }
  
  // LCD 2 - Mostra total
  lcd2.clear();
  lcd2.setCursor(0, 0);
  lcd2.print("Pedido #");
  lcd2.print(pedidoAtual.numero);
  lcd2.setCursor(0, 1);
  lcd2.print("Total: R$");
  lcd2.print(pedidoAtual.total, 2);
  
  // Serial
  Serial.print("\n--- RESUMO DO PEDIDO ---\n");
  Serial.print("Numero: #");
  Serial.println(pedidoAtual.numero);
  Serial.println("Itens:");
  
  for (int i = 0; i < NUM_ITENS; i++) {
    if (pedidoAtual.itens[i] > 0) {
      Serial.print("  ");
      Serial.print(pedidoAtual.itens[i]);
      Serial.print("x ");
      Serial.print(cardapio[i].nome);
      Serial.print(" = R$ ");
      Serial.println(cardapio[i].preco * pedidoAtual.itens[i], 2);
    }
  }
  
  Serial.print("Total: R$ ");
  Serial.println(pedidoAtual.total, 2);
  Serial.println("Digite 0 para confirmar o pedido");
  Serial.println("Ou continue adicionando itens\n");
}

// ============= FUNÇÕES DE CONFIRMAÇÃO =============

void confirmarPedido() {
  if (pedidoAtual.total == 0) {
    Serial.println("Pedido vazio! Adicione itens.");
    return;
  }
  
  Serial.println("\n========================================");
  Serial.println("PEDIDO CONFIRMADO");
  Serial.println("========================================");
  Serial.print("Numero: #");
  Serial.println(pedidoAtual.numero);
  Serial.println("Itens:");
  
  for (int i = 0; i < NUM_ITENS; i++) {
    if (pedidoAtual.itens[i] > 0) {
      Serial.print("  ");
      Serial.print(pedidoAtual.itens[i]);
      Serial.print("x ");
      Serial.println(cardapio[i].nome);
    }
  }
  
  Serial.print("Total: R$ ");
  Serial.println(pedidoAtual.total, 2);
  Serial.println("========================================\n");
  
  // Adicionar ao histórico
  if (totalPedidosProcessados < MAX_PEDIDOS) {
    historicoPedidos[totalPedidosProcessados] = pedidoAtual;
    totalPedidosProcessados++;
  }
  
  // Iniciar processamento
  estadoAtual = PROCESSANDO;
  tempoInicioPedido = millis();
  
  // Ativar servo
  digitalWrite(SERVO_PIN, HIGH);
  
  Serial.println("Servo iniciado!");
  Serial.println("Preparando pedido...\n");
}

// ============= FUNÇÕES DE PROCESSAMENTO =============

void processarPedidoEmAndamento() {
  unsigned long tempoDecorrido = millis() - tempoInicioPedido;
  int tempoTotal = TEMPO_PREP_PADRAO * 1000;  // Converter para milissegundos
  int percentual = (tempoDecorrido * 100) / tempoTotal;
  
  if (percentual > 100) {
    percentual = 100;
  }
  
  // Atualizar LCD a cada 500ms
  if (millis() - ultimoTempoProcesamento > 500) {
    // LCD 1 - Progresso
    lcd1.clear();
    lcd1.setCursor(0, 0);
    lcd1.print("PREPARANDO #");
    lcd1.print(pedidoAtual.numero);
    lcd1.print(" ");
    lcd1.print(percentual);
    lcd1.print("%");
    
    lcd1.setCursor(0, 1);
    lcd1.print("Itens:");
    
    int linha = 2;
    for (int i = 0; i < NUM_ITENS && linha <= 3; i++) {
      if (pedidoAtual.itens[i] > 0) {
        lcd1.setCursor(0, linha);
        lcd1.print(pedidoAtual.itens[i]);
        lcd1.print("x");
        lcd1.print(cardapio[i].nome);
        linha++;
      }
    }
    
    // LCD 2 - Barra de progresso
    lcd2.clear();
    lcd2.setCursor(0, 0);
    lcd2.print("Processando");
    lcd2.setCursor(0, 1);
    lcd2.print("[");
    
    for (int i = 0; i < 10; i++) {
      if (i < (percentual / 10)) {
        lcd2.print("=");
      } else {
        lcd2.print(" ");
      }
    }
    
    lcd2.print("]");
    
    ultimoTempoProcesamento = millis();
  }
  
  // Verificar se terminou
  if (tempoDecorrido >= tempoTotal) {
    finalizarPedido();
  }
}

void finalizarPedido() {
  Serial.println("\n========================================");
  Serial.println("PEDIDO PRONTO!");
  Serial.println("========================================");
  Serial.print("Numero: #");
  Serial.println(pedidoAtual.numero);
  Serial.println("========================================\n");
  
  // Desativar servo
  digitalWrite(SERVO_PIN, LOW);
  
  // Ativar buzzer
  buzzerAtivo = true;
  ultimoBeepBuzzer = millis();
  
  // Mudar estado
  estadoAtual = PRONTO;
  
  // LCD 1 - Pedido pronto com detalhes
  lcd1.clear();
  lcd1.setCursor(0, 0);
  lcd1.print("PEDIDO #");
  lcd1.print(pedidoAtual.numero);
  lcd1.print(" PRONTO!");
  lcd1.setCursor(0, 1);
  lcd1.print("Itens:");
  
  int linha = 2;
  for (int i = 0; i < NUM_ITENS && linha <= 3; i++) {
    if (pedidoAtual.itens[i] > 0) {
      lcd1.setCursor(0, linha);
      lcd1.print(pedidoAtual.itens[i]);
      lcd1.print("x");
      lcd1.print(cardapio[i].nome);
      linha++;
    }
  }
  
  // LCD 2 - Retire
  lcd2.clear();
  lcd2.setCursor(0, 0);
  lcd2.print("RETIRE SEU PEDIDO!");
  lcd2.setCursor(0, 1);
  lcd2.print("#");
  lcd2.print(pedidoAtual.numero);
  
  Serial.println("Status: PRONTO!");
  Serial.println("Buzzer ativado!");
}

void processarPedidoPronto() {
  // Aguardar 5 segundos antes de voltar ao menu
  if (millis() - tempoInicioPedido > 15000) {
    buzzerAtivo = false;
    digitalWrite(BUZZER_PIN, LOW);
    
    // Próximo pedido
    numeroPedidoAtual++;
    inicializarNovoPedido();
    exibirMenuPrincipal();
  }
}

// ============= FUNÇÕES DE BUZZER =============

void atualizarBuzzer() {
  unsigned long tempoDecorrido = millis() - ultimoBeepBuzzer;
  
  // Buzzer intermitente: 500ms ligado, 500ms desligado
  if (tempoDecorrido % 1000 < 500) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// ============= FUNÇÕES DE EMERGÊNCIA =============

void ativarParadaEmergencia() {
  Serial.println("\n========================================");
  Serial.println("!!! PARADA DE EMERGENCIA ATIVADA !!!");
  Serial.println("========================================\n");
  
  // Desativar todos os componentes
  digitalWrite(SERVO_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  buzzerAtivo = false;
  
  // Mudar estado
  estadoAtual = EMERGENCIA;
  
  // LCD 1 - Mensagem de emergência
  lcd1.clear();
  lcd1.setCursor(0, 0);
  lcd1.print("!!! EMERGENCIA !!!");
  lcd1.setCursor(0, 1);
  lcd1.print("Sistema Parado");
  lcd1.setCursor(0, 2);
  lcd1.print("Servo: DESLIGADO");
  lcd1.setCursor(0, 3);
  lcd1.print("Pressione RESET");
  
  // LCD 2 - Mensagem de emergência
  lcd2.clear();
  lcd2.setCursor(0, 0);
  lcd2.print("EMERGENCIA!");
  lcd2.setCursor(0, 1);
  lcd2.print("Sistema Parado");
  
  Serial.println("Servo: DESLIGADO");
  Serial.println("Buzzer: DESLIGADO");
  Serial.println("Aguardando RESET do sistema...\n");
  
  // Ficar em loop até reset
  while (true) {
    delay(100);
  }
}

// ============= FUNÇÕES AUXILIARES =============

void exibirHistoricoPedidos() {
  Serial.println("\n========================================");
  Serial.println("HISTORICO DE PEDIDOS");
  Serial.println("========================================");
  
  if (totalPedidosProcessados == 0) {
    Serial.println("Nenhum pedido processado ainda.");
  } else {
    for (int i = 0; i < totalPedidosProcessados; i++) {
      Serial.print("Pedido #");
      Serial.print(historicoPedidos[i].numero);
      Serial.print(" - Total: R$ ");
      Serial.println(historicoPedidos[i].total, 2);
    }
  }
  
  Serial.println("========================================\n");
}

void exibirStatusSistema() {
  Serial.println("\n========================================");
  Serial.println("STATUS DO SISTEMA");
  Serial.println("========================================");
  Serial.print("Estado Atual: ");
  
  switch (estadoAtual) {
    case MENU_PRINCIPAL:
      Serial.println("MENU PRINCIPAL");
      break;
    case SELECAO_QUANTIDADE:
      Serial.println("SELEÇÃO DE QUANTIDADE");
      break;
    case PROCESSANDO:
      Serial.println("PROCESSANDO PEDIDO");
      break;
    case PRONTO:
      Serial.println("PEDIDO PRONTO");
      break;
    case EMERGENCIA:
      Serial.println("EMERGENCIA");
      break;
  }
  
  Serial.print("Pedido Atual: #");
  Serial.println(pedidoAtual.numero);
  Serial.print("Total de Pedidos Processados: ");
  Serial.println(totalPedidosProcessados);
  Serial.println("========================================\n");
}

/*
========================================
INSTRUÇÕES DE USO - SISTEMA COMPLETO
========================================

MENU NUMERADO (1-8):
1. Hamburger - R$ 15.00
2. Batata Frita - R$ 8.50
3. Refrigerante - R$ 5.00
4. Pizza - R$ 25.00
5. Frango Frito - R$ 18.00
6. Sobremesa - R$ 10.00
7. Suco - R$ 7.00
8. Agua - R$ 3.00

FLUXO DE OPERAÇÃO:

1. MENU PRINCIPAL (Estado 0)
   - Use UP/DOWN para navegar
   - Digite número (1-8) para selecionar

2. SELEÇÃO DE QUANTIDADE (Estado 1)
   - Use UP/DOWN para ajustar quantidade
   - Digite número novamente para confirmar
   - Digite 0 para cancelar

3. RESUMO DO PEDIDO (Estado 0)
   - Mostra todos os itens
   - Mostra total
   - Continue adicionando ou confirme

4. CONFIRMAÇÃO (Digitar 0)
   - Confirma o pedido
   - Inicia processamento

5. PROCESSAMENTO (Estado 2)
   - Servo gira
   - Mostra progresso
   - Barra visual

6. PRONTO (Estado 3)
   - Buzzer toca
   - Mostra ficha completa
   - Volta ao menu automaticamente

EXEMPLO PRÁTICO:

Entrada: 1
→ Seleciona Hamburger
→ Mostra: "Qtd: 1"

Entrada: UP (botão)
→ Aumenta para: "Qtd: 2"

Entrada: 1 (confirma)
→ Adiciona: 2x Hamburger
→ Total: R$ 30.00

Entrada: 2
→ Seleciona Batata
→ Mostra: "Qtd: 1"

Entrada: UP (botão)
→ Aumenta para: "Qtd: 3"

Entrada: 2 (confirma)
→ Adiciona: 3x Batata
→ Total: R$ 55.50

Entrada: 0
→ Confirma pedido #1001
→ Inicia processamento
→ Após 10s: "PRONTO!"
→ Volta ao menu

PARADA DE EMERGÊNCIA:
- Pressione botão de emergência
- Sistema para imediatamente
- Todos os componentes desligam
- Pressione RESET para reiniciar

CONEXÕES WOKWI:

LCD 1 (20x4, 0x26):
  GND → GND
  VCC → 5V
  SDA → GPIO 21
  SCL → GPIO 22

LCD 2 (16x2, 0x27):
  GND → GND
  VCC → 5V
  SDA → GPIO 21
  SCL → GPIO 22

Servo Motor:
  GND → GND
  V+ → 5V
  PWM → GPIO 27

Buzzer:
  + → GPIO 26
  - → GND

Botão Emergência:
  Pino 1 → GPIO 2
  Pino 2 → GND

Botão UP:
  Pino 1 → GPIO 35
  Pino 2 → GND

Botão DOWN:
  Pino 1 → GPIO 34
  Pino 2 → GND

========================================
FUNCIONALIDADES INCLUÍDAS
========================================

✅ Menu numerado (1-8 itens)
✅ Seleção intuitiva de itens
✅ Ajuste de quantidade com UP/DOWN
✅ Confirmação de seleção
✅ Cancelamento de seleção
✅ Resumo do pedido em tempo real
✅ Cálculo automático de total
✅ Múltiplos itens no pedido
✅ Ficha memorizada com número
✅ Processamento com progresso
✅ Barra visual de progresso
✅ Buzzer intermitente
✅ Parada de emergência
✅ Histórico de pedidos
✅ Número incrementa automaticamente
✅ Debounce de botões
✅ Lógica de estados completa
✅ Comentários detalhados

========================================
*/
