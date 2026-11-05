#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "TAMC_GT911.h"
#include "qrcode.h"

/* --- CONFIGURAÇÃO WI-FI E LNBITS --- */
const char* ssid = "";     // <-- COLE SEU SSID AQUI
const char* password = ""; // <-- COLE SUA SENHA AQUI

String lnbits_url = "https://engine-1.hexarcs.com/api/v1/payments";
String lnbits_wallet_url = "https://engine-1.hexarcs.com/api/v1/wallet";
String lnbits_api_key = ""; // <-- COLE SUA CHAVE X-Api-Key AQUI

/* --- DEFINIÇÕES DE PINOS DO TOUCH CAPACITIVO (GT911) --- */
#define TOUCH_SDA 33
#define TOUCH_SCL 32
#define TOUCH_INT 21
#define TOUCH_RST 25
#define TOUCH_WIDTH 240
#define TOUCH_HEIGHT 320

/* --- OBJETOS DE HARDWARE --- */
TAMC_GT911 tp = TAMC_GT911(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST, TOUCH_WIDTH, TOUCH_HEIGHT);
TFT_eSPI tft = TFT_eSPI();

/* --- ESTADOS DAS TELAS --- */
enum ScreenState {
  SCREEN_CONNECTING,
  SCREEN_KEYPAD,
  SCREEN_LOADING,
  SCREEN_QR_CODE,
  SCREEN_PAID,
  SCREEN_ERROR
};

ScreenState currentScreen = SCREEN_CONNECTING;

/* --- VARIÁVEIS GLOBAIS --- */
String input_amount = "";
long current_sats = 0;
String current_payment_hash = "";
String current_bolt11 = "";

unsigned long lastTouchTime = 0;
unsigned long lastAutoCheckTime = 0;
unsigned long paidScreenTime = 0;
bool wasTouched = false;

/* --- DECLARAÇÃO DE FUNÇÕES --- */
void kickstartHardware();
void desenharBotao(int x, int y, int w, int h, const char* texto, uint16_t corFundo, uint16_t corTexto = 0xFFFF, uint8_t textSize = 2);
void atualizarVisor();
void desenharTelaTeclado();
void desenharTelaCarregando();
void updateQRStatus(String msg, uint16_t color);
void desenharTelaQRCode();
void desenharTelaPago();
void desenharTelaErro(String titulo, String detalhe);
void connectWiFi();
void checkBalance();
void createInvoice(int sats);
void manualCheck();
void verificarPagamentoSilencioso();
void adicionarDigito(char d);
void apagarDigito();
void processarOK();
void tratarToque(int px, int py);
void verificarTouch();
void mostrarInformacoesSistema();

/* --- FUNÇÕES DE HARDWARE --- */
void kickstartHardware() {
  #ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  #endif
}

/* --- FUNÇÃO AUXILIAR PARA DESENHAR BOTÕES --- */
void desenharBotao(int x, int y, int w, int h, const char* texto, uint16_t corFundo, uint16_t corTexto, uint8_t textSize) {
  tft.fillRoundRect(x, y, w, h, 6, corFundo);
  tft.drawRoundRect(x, y, w, h, 6, 0xFFFF);
  tft.setTextColor(corTexto, corFundo);
  tft.setTextSize(textSize);

  int textWidth = strlen(texto) * 6 * textSize;
  int textHeight = 8 * textSize;
  int tx = x + (w - textWidth) / 2;
  int ty = y + (h - textHeight) / 2;
  tft.setCursor(tx, ty);
  tft.print(texto);
}

/* --- ATUALIZAÇÃO DO VISOR DO TECLADO (VERTICAL 240x320) --- */
void atualizarVisor() {
  tft.fillRoundRect(10, 40, 220, 54, 4, 0x0000);
  tft.drawRoundRect(10, 40, 220, 54, 4, 0x4A69);

  tft.setCursor(20, 52);
  if (input_amount.length() > 0) {
    tft.setTextColor(0x07E0, 0x0000); // Verde brilhante
    tft.setTextSize(3);
    tft.print(input_amount);
  } else {
    tft.setTextColor(0x7BEF, 0x0000); // Cinza
    tft.setTextSize(3);
    tft.print("0");
  }

  tft.setTextSize(2);
  tft.setTextColor(0xFFFF, 0x0000);
  tft.setCursor(20, 100);
  tft.print("SATS");
}

/* --- TELA 1: TECLADO PDV VERTICAL --- */
void desenharTelaTeclado() {
  currentScreen = SCREEN_KEYPAD;
  tft.fillScreen(0x0000);

  tft.setTextColor(0xFDA0, 0x0000);
  tft.setTextSize(2);
  tft.drawString("KIX POS", 10, 10);

  tft.setTextSize(1);
  if (WiFi.status() == WL_CONNECTED) {
    tft.setTextColor(0x07E0, 0x0000);
    tft.drawString("ONLINE", 180, 14);
  } else {
    tft.setTextColor(0xF800, 0x0000);
    tft.drawString("OFFLINE", 175, 14);
  }

  atualizarVisor();

  // Teclado numérico em layout vertical (3 colunas x 4 linhas)
  // Largura dos botões: ~66px cada com espaçamento
  // Linha 1: [ 1 ] [ 2 ] [ 3 ]
  desenharBotao(10,  130, 66, 42, "1", 0x2124, 0xFFFF, 3);
  desenharBotao(87,  130, 66, 42, "2", 0x2124, 0xFFFF, 3);
  desenharBotao(164, 130, 66, 42, "3", 0x2124, 0xFFFF, 3);

  // Linha 2: [ 4 ] [ 5 ] [ 6 ]
  desenharBotao(10,  178, 66, 42, "4", 0x2124, 0xFFFF, 3);
  desenharBotao(87,  178, 66, 42, "5", 0x2124, 0xFFFF, 3);
  desenharBotao(164, 178, 66, 42, "6", 0x2124, 0xFFFF, 3);

  // Linha 3: [ 7 ] [ 8 ] [ 9 ]
  desenharBotao(10,  226, 66, 42, "7", 0x2124, 0xFFFF, 3);
  desenharBotao(87,  226, 66, 42, "8", 0x2124, 0xFFFF, 3);
  desenharBotao(164, 226, 66, 42, "9", 0x2124, 0xFFFF, 3);

  // Linha 4: [ DEL ] [ 0 ] [ OK ]
  desenharBotao(10,  274, 66, 38, "DEL", 0x8800, 0xFFFF, 2);
  desenharBotao(87,  274, 66, 38, "0",   0x2124, 0xFFFF, 3);
  desenharBotao(164, 274, 66, 38, "OK",  0x05E0, 0xFFFF, 3);
}

/* --- TELA: CARREGANDO FATURA --- */
void desenharTelaCarregando() {
  currentScreen = SCREEN_LOADING;
  tft.fillScreen(0x0000);
  tft.setTextColor(0x07FF, 0x0000);
  tft.setTextSize(2);
  tft.drawCentreString("CRIANDO INVOICE...", 120, 100, 1);

  tft.setTextColor(0xFFE0, 0x0000);
  tft.setTextSize(3);
  tft.drawCentreString(String(current_sats) + " SATS", 120, 140, 1);

  tft.setTextColor(0x7BEF, 0x0000);
  tft.setTextSize(1);
  tft.drawCentreString("Aguarde...", 120, 190, 1);
}

/* --- TELA 2: QR CODE DA FATURA (VERTICAL) --- */
void updateQRStatus(String msg, uint16_t color) {
  tft.fillRect(10, 250, 220, 20, 0x0000);
  tft.setTextColor(color, 0x0000);
  tft.setTextSize(1);
  tft.drawCentreString(msg, 120, 255, 1);
}

void desenharTelaQRCode() {
  currentScreen = SCREEN_QR_CODE;
  tft.fillScreen(0xFFFF); // fundo branco para facilitar a leitura do QR pelas cameras

  QRCode qrcode;
  uint8_t qrcodeData[800];
  const uint8_t qrVersion = 14;

  if (current_bolt11.length() > 450) {
    tft.setTextColor(0xF800, 0xFFFF);
    tft.setTextSize(1);
    tft.drawCentreString("INVOICE MUITO LONGA", 120, 100, 1);
    desenharBotao(20, 270, 200, 40, "VOLTAR", 0x9000, 0xFFFF, 2);
    return;
  }

  int8_t qrResult = qrcode_initText(&qrcode, qrcodeData, qrVersion, ECC_LOW, current_bolt11.c_str());

  if (qrResult != 0) {
    updateQRStatus("Erro ao gerar QR", 0xF800);
    desenharBotao(20, 270, 200, 40, "VOLTAR", 0x9000, 0xFFFF, 2);
    return;
  }

  // Renderiza o QR Code centralizado na parte superior
  const int scale = 2;
  const int quietZone = 2;
  const int totalPixels = (qrcode.size + 2 * quietZone) * scale;
  const int xOffset = (240 - totalPixels) / 2;
  const int yOffset = 10;

  tft.fillRect(xOffset, yOffset, totalPixels, totalPixels, 0xFFFF);

  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        tft.fillRect(xOffset + (x + quietZone) * scale, yOffset + (y + quietZone) * scale, scale, scale, 0x0000);
      }
    }
  }

  // Valor cobrado abaixo do QR Code
  tft.setTextColor(0x0000, 0xFFFF);
  tft.setTextSize(2);
  tft.drawCentreString(String(current_sats) + " SATS", 120, 225, 1);

  updateQRStatus("Aguardando pagamento...", 0x07FF);

  // Botões inferiores: CHECAR e VOLTAR lado a lado
  desenharBotao(10,  275, 105, 38, "CHECAR", 0x03E0, 0xFFFF, 2);
  desenharBotao(125, 275, 105, 38, "VOLTAR", 0x9000, 0xFFFF, 2);
}

/* --- TELA 3: PAGAMENTO CONFIRMADO --- */
void desenharTelaPago() {
  currentScreen = SCREEN_PAID;
  paidScreenTime = millis();
  tft.fillScreen(0x05E0);

  tft.setTextColor(0xFFFF, 0x05E0);
  tft.setTextSize(3);
  tft.drawCentreString("PAGO COM", 120, 50, 1);
  tft.drawCentreString("SUCESSO!", 120, 90, 1);

  tft.setTextColor(0xFFE0, 0x05E0);
  tft.setTextSize(3);
  tft.drawCentreString(String(current_sats) + " SATS", 120, 150, 1);

  tft.setTextSize(1);
  tft.drawCentreString("Toque na tela para continuar", 120, 240, 1);
}

/* --- TELA DE ERRO --- */
void desenharTelaErro(String titulo, String detalhe) {
  currentScreen = SCREEN_ERROR;
  tft.fillScreen(0x0000);
  tft.setTextColor(0xF800, 0x0000);
  tft.setTextSize(2);
  tft.drawCentreString(titulo, 120, 60, 1);

  tft.setTextColor(0xFFFF, 0x0000);
  tft.setTextSize(1);
  tft.drawCentreString(detalhe.substring(0, 35), 120, 120, 1);

  desenharBotao(20, 240, 200, 40, "VOLTAR", 0x9000, 0xFFFF, 2);
}

/* --- LÓGICA DE ENTRADA DO TECLADO --- */
void adicionarDigito(char d) {
  if (input_amount.length() >= 8) return;
  if (input_amount == "0") {
    if (d == '0') return;
    input_amount = String(d);
  } else {
    input_amount += d;
  }
  atualizarVisor();
}

void apagarDigito() {
  if (input_amount.length() > 0) {
    input_amount.remove(input_amount.length() - 1);
  }
  atualizarVisor();
}

void processarOK() {
  long sats = input_amount.toInt();
  if (sats <= 0) {
    tft.fillRoundRect(10, 40, 220, 54, 4, 0x8000);
    tft.drawRoundRect(10, 40, 220, 54, 4, 0xF800);
    tft.setTextColor(0xFFFF, 0x8000);
    tft.setTextSize(2);
    tft.setCursor(20, 56);
    tft.print("DIGITE VALOR!");
    delay(800);
    atualizarVisor();
    return;
  }

  current_sats = sats;
  desenharTelaCarregando();
  createInvoice(sats);
}

/* --- REDE E COMUNICAÇÃO LNBITS --- */
void connectWiFi() {
  currentScreen = SCREEN_CONNECTING;
  input_amount = "";
  current_bolt11 = "";
  current_payment_hash = "";

  tft.fillScreen(0x0000);
  tft.setTextColor(0xFDA0, 0x0000);
  tft.setTextSize(3);
  tft.setCursor(20, 40);
  tft.println("KIX POS");

  tft.setTextSize(2);
  tft.setCursor(20, 100);
  tft.setTextColor(0xFFFF, 0x0000);
  tft.println("Conectando WiFi...");
  tft.setTextSize(1);
  tft.setTextColor(0x7BEF, 0x0000);
  tft.setCursor(20, 135);
  tft.println(ssid);

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
      delay(500);
      kickstartHardware();
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    tft.setTextColor(0x07E0, 0x0000);
    tft.setTextSize(2);
    tft.setCursor(20, 170);
    tft.println("Conectado!");
    delay(800);
  } else {
    tft.setTextColor(0xF800, 0x0000);
    tft.setTextSize(2);
    tft.setCursor(20, 170);
    tft.println("Offline!");
    delay(1200);
  }

  desenharTelaTeclado();
}

// Cliente HTTPS: modo insecure usado aqui para diagnóstico.
// Depois de confirmar a comunicação, recomendamos validar o certificado TLS.
void checkBalance() {
  Serial.println("\n[WALLET] Iniciando consulta de saldo...");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WALLET][ERRO] Wi-Fi desconectado.");
    desenharTelaErro("SEM WIFI", "Verifique a conexao");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15000);

  HTTPClient http;
  http.setTimeout(15000);
  http.setReuse(false);

  Serial.print("[WALLET] GET ");
  Serial.println(lnbits_wallet_url);

  if (!http.begin(client, lnbits_wallet_url)) {
    Serial.println("[WALLET][ERRO] http.begin() falhou.");
    desenharTelaErro("ERRO HTTP", "Falha ao iniciar");
    return;
  }

  http.addHeader("X-Api-Key", lnbits_api_key);

  Serial.println("[WALLET] Enviando requisicao...");
  int code = http.GET();

  Serial.print("[WALLET] HTTP status: ");
  Serial.println(code);

  if (code > 0) {
    String raw = http.getString();
    Serial.println("[WALLET] Resposta recebida:");
    Serial.println(raw);

    if (code == 200) {
      DynamicJsonDocument doc(2048);
      DeserializationError jsonError = deserializeJson(doc, raw);

      if (jsonError) {
        Serial.print("[WALLET][ERRO] JSON invalido: ");
        Serial.println(jsonError.c_str());
        http.end();
        desenharTelaErro("ERRO JSON", "Resposta invalida");
        return;
      }

      if (doc["balance_msat"].isNull()) {
        Serial.println("[WALLET][ERRO] Campo balance_msat ausente.");
        http.end();
        desenharTelaErro("ERRO JSON", "Saldo nao encontrado");
        return;
      }

      long balance = doc["balance_msat"].as<long>() / 1000;

      Serial.print("[WALLET] Saldo: ");
      Serial.print(balance);
      Serial.println(" sats");

      http.end();

      tft.fillScreen(0x0000);
      tft.setCursor(10, 40);
      tft.setTextColor(0xFDA0);
      tft.setTextSize(2);
      tft.println("SALDO DA CARTEIRA:");
      tft.setCursor(10, 110);
      tft.setTextColor(0xFFFF);
      tft.setTextSize(3);
      tft.print(balance);
      tft.setTextSize(2);
      tft.println(" SATS");
      delay(4000);
      desenharTelaTeclado();
      return;
    }
  } else {
    Serial.print("[WALLET][ERRO] Falha de transporte: ");
    Serial.println(http.errorToString(code));
  }

  http.end();
  desenharTelaErro("ERRO NA CARTEIRA", "HTTP " + String(code));
}


void createInvoice(int sats) {
  Serial.println("\n[INVOICE] ===== INICIO =====");
  Serial.print("[INVOICE] Valor solicitado: ");
  Serial.print(sats);
  Serial.println(" sats");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[INVOICE][ERRO] Wi-Fi desconectado.");
    desenharTelaErro("SEM WIFI", "Verifique a conexao");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(20000);

  HTTPClient http;
  http.setTimeout(20000);
  http.setReuse(false);

  Serial.print("[INVOICE] Endpoint: ");
  Serial.println(lnbits_url);

  if (!http.begin(client, lnbits_url)) {
    Serial.println("[INVOICE][ERRO] http.begin() falhou.");
    desenharTelaErro("ERRO HTTP", "Falha ao iniciar");
    return;
  }

  http.addHeader("X-Api-Key", lnbits_api_key);
  http.addHeader("Content-Type", "application/json");

  String payload = "{\"out\": false, \"amount\":" + String(sats) + ", \"memo\": \"KIX-POS\"}";

  Serial.print("[INVOICE] Payload: ");
  Serial.println(payload);
  Serial.println("[INVOICE] Enviando POST...");

  int httpCode = http.POST(payload);

  Serial.print("[INVOICE] HTTP status: ");
  Serial.println(httpCode);

  String response = "";

  if (httpCode > 0) {
    response = http.getString();
    Serial.println("[INVOICE] Resposta da API:");
    Serial.println(response);
  } else {
    Serial.print("[INVOICE][ERRO] Falha de transporte: ");
    Serial.println(http.errorToString(httpCode));
  }

  if (httpCode == 200 || httpCode == 201) {
    DynamicJsonDocument doc(4096);
    DeserializationError jsonError = deserializeJson(doc, response);

    if (jsonError) {
      Serial.print("[INVOICE][ERRO] JSON invalido: ");
      Serial.println(jsonError.c_str());
      http.end();
      desenharTelaErro("ERRO JSON", "Resposta invalida");
      return;
    }

    current_bolt11 = doc["payment_request"].as<String>();
    current_payment_hash = doc["payment_hash"].as<String>();

    if (current_bolt11.length() == 0 || current_payment_hash.length() == 0) {
      Serial.println("[INVOICE][ERRO] API nao retornou payment_request/payment_hash.");
      http.end();
      desenharTelaErro("ERRO INVOICE", "Campos ausentes");
      return;
    }

    Serial.println("[INVOICE] Invoice criada com sucesso.");
    Serial.print("[INVOICE] BOLT11 recebido (");
    Serial.print(current_bolt11.length());
    Serial.println(" caracteres).");
    Serial.println("[INVOICE] Exibindo QR Code...");

    http.end();
    desenharTelaQRCode();

    Serial.println("[INVOICE] ===== FIM OK =====");
    return;
  }

  http.end();
  Serial.println("[INVOICE] ===== FIM COM ERRO =====");
  desenharTelaErro("ERRO NA FATURA", "HTTP " + String(httpCode));
}


void manualCheck() {
  Serial.println("\n[CHECK] Verificacao manual solicitada.");

  if (current_payment_hash == "") {
    Serial.println("[CHECK] Nao existe invoice ativa.");
    updateQRStatus("SEM INVOICE", 0xF800);
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[CHECK][ERRO] Wi-Fi desconectado.");
    updateQRStatus("SEM WIFI", 0xF800);
    return;
  }

  updateQRStatus("CHECANDO...", 0xFFE0);

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15000);

  HTTPClient http;
  http.setTimeout(15000);
  http.setReuse(false);

  String checkUrl = lnbits_url + "/" + current_payment_hash;

  Serial.print("[CHECK] GET ");
  Serial.println(checkUrl);

  if (!http.begin(client, checkUrl)) {
    Serial.println("[CHECK][ERRO] http.begin() falhou.");
    updateQRStatus("ERRO AO CONECTAR", 0xF800);
    return;
  }

  http.addHeader("X-Api-Key", lnbits_api_key);

  Serial.println("[CHECK] Enviando GET...");
  int code = http.GET();

  Serial.print("[CHECK] HTTP status: ");
  Serial.println(code);

  if (code == 200) {
    String raw = http.getString();
    Serial.println("[CHECK] Resposta da API:");
    Serial.println(raw);

    DynamicJsonDocument doc(2048);
    DeserializationError jsonError = deserializeJson(doc, raw);

    if (jsonError) {
      Serial.print("[CHECK][ERRO] JSON invalido: ");
      Serial.println(jsonError.c_str());
      http.end();
      updateQRStatus("ERRO NA RESPOSTA", 0xF800);
      return;
    }

    bool paid = doc["paid"].as<bool>();
    http.end();

    if (paid) {
      Serial.println("[CHECK] PAGAMENTO CONFIRMADO!");
      desenharTelaPago();
      return;
    }

    Serial.println("[CHECK] Invoice ainda nao foi paga.");
    updateQRStatus("NAO PAGO AINDA", 0xFDA0);
    return;
  }

  if (code > 0) {
    String raw = http.getString();
    Serial.println("[CHECK] Resposta de erro:");
    Serial.println(raw);
  } else {
    Serial.print("[CHECK][ERRO] Falha de transporte: ");
    Serial.println(http.errorToString(code));
  }

  http.end();
  updateQRStatus("ERRO: " + String(code), 0xF800);
}


void verificarPagamentoSilencioso() {
  if (current_payment_hash == "") return;

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[AUTO-CHECK] Wi-Fi desconectado; verificacao ignorada.");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(12000);

  HTTPClient http;
  http.setTimeout(12000);
  http.setReuse(false);

  String checkUrl = lnbits_url + "/" + current_payment_hash;

  if (!http.begin(client, checkUrl)) {
    Serial.println("[AUTO-CHECK][ERRO] http.begin() falhou.");
    return;
  }

  http.addHeader("X-Api-Key", lnbits_api_key);

  int code = http.GET();

  if (code == 200) {
    String raw = http.getString();
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, raw);

    if (!error && doc["paid"].as<bool>()) {
      Serial.println("[AUTO-CHECK] PAGAMENTO CONFIRMADO!");
      http.end();
      desenharTelaPago();
      return;
    }

    if (error) {
      Serial.print("[AUTO-CHECK][ERRO] JSON: ");
      Serial.println(error.c_str());
    }
  } else if (code < 0) {
    Serial.print("[AUTO-CHECK][ERRO] ");
    Serial.println(http.errorToString(code));
  }

  http.end();
}


void mostrarInformacoesSistema() {
  Serial.println("\n========== KIX POS / STATUS ==========");

  Serial.print("Estado: ");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("ONLINE");
  } else {
    Serial.println("OFFLINE");
  }

  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());

  Serial.print("Mascara: ");
  Serial.println(WiFi.subnetMask());

  Serial.print("DNS: ");
  Serial.println(WiFi.dnsIP());

  Serial.print("MAC: ");
  Serial.println(WiFi.macAddress());

  Serial.print("Sinal Wi-Fi (RSSI): ");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("indisponivel");
  }

  Serial.print("Tempo ligado: ");
  Serial.print(millis() / 1000);
  Serial.println(" segundos");

  Serial.print("Heap livre: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");

  Serial.print("PSRAM livre: ");
  Serial.print(ESP.getFreePsram());
  Serial.println(" bytes");

  Serial.print("LNbits endpoint: ");
  Serial.println(lnbits_url);

  Serial.print("Invoice ativa: ");
  Serial.println(current_payment_hash.length() > 0 ? "SIM" : "NAO");

  Serial.println("======================================");
}

/* --- TRATAMENTO DE TOQUE CAPACITIVO (VERTICAL) --- */
void tratarToque(int px, int py) {
  if (currentScreen == SCREEN_KEYPAD) {
    // Linha 1: 1, 2, 3 (y: 130 a 172)
    if (py >= 130 && py < 174) {
      if (px < 79) adicionarDigito('1');
      else if (px < 156) adicionarDigito('2');
      else adicionarDigito('3');
    }
    // Linha 2: 4, 5, 6 (y: 178 a 220)
    else if (py >= 174 && py < 222) {
      if (px < 79) adicionarDigito('4');
      else if (px < 156) adicionarDigito('5');
      else adicionarDigito('6');
    }
    // Linha 3: 7, 8, 9 (y: 226 a 268)
    else if (py >= 222 && py < 270) {
      if (px < 79) adicionarDigito('7');
      else if (px < 156) adicionarDigito('8');
      else adicionarDigito('9');
    }
    // Linha 4: DEL, 0, OK (y: 274 a 320)
    else if (py >= 270 && py <= 320) {
      if (px < 79) apagarDigito();
      else if (px < 156) adicionarDigito('0');
      else processarOK();
    }
  }
  else if (currentScreen == SCREEN_QR_CODE) {
    // Botões inferiores na tela de QR Code (y >= 270)
    if (py >= 270) {
      if (px < 120) {
        manualCheck();
      } else {
        current_payment_hash = "";
        current_bolt11 = "";
        input_amount = "";
        desenharTelaTeclado();
      }
    }
  }
  else if (currentScreen == SCREEN_PAID || currentScreen == SCREEN_ERROR) {
    input_amount = "";
    current_payment_hash = "";
    current_bolt11 = "";
    desenharTelaTeclado();
  }
}

void verificarTouch() {
  tp.read();
  if (tp.isTouched) {
    if (!wasTouched && (millis() - lastTouchTime > 180)) {
      wasTouched = true;
      lastTouchTime = millis();

      int px = tp.points[0].x;
      int py = tp.points[0].y;

      // Restrições para o modo vertical (240x320)
      px = constrain(px, 0, 239);
      py = constrain(py, 0, 319);

      tratarToque(px, py);
    }
  } else {
    wasTouched = false;
  }
}

/* --- SETUP --- */
void setup() {
  Serial.begin(115200);
  delay(500); // Pequena pausa para estabilizar a Serial
  
  Serial.println("\n[BOOT] Inicializando KIX POS...");
  Serial.println("[BOOT] Serial pronta em 115200 baud.");
  
  kickstartHardware();

  tft.init();
  tft.setRotation(2); // Orientação Vertical (Portrait)
  tft.setTextWrap(false);
  tft.fillScreen(0x0000);

  // Inicializa o barramento I2C explicitamente para os pinos do CYD GT911
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  
  // Inicializa o Touch Capacitivo GT911
  tp.begin();
  tp.setRotation(ROTATION_NORMAL);

  connectWiFi();
}

/* --- VARIÁVEL GLOBAL PARA O TIMER DE DEBUG --- */
unsigned long lastDebugPrintTime = 0;

/* --- LOOP PRINCIPAL --- */

void loop() {

  // =====================================================
  // 1. RECEPCAO SERIAL - NAO BLOQUEANTE
  // =====================================================

  static String serialInput = "";

  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      serialInput.trim();

      if (serialInput.length() > 0) {
        Serial.print("\n[SERIAL RX] Comando recebido: [");
        Serial.print(serialInput);
        Serial.println("]");

        // I:VALOR - criar invoice
        if (serialInput.startsWith("I:")) {
          String valor = serialInput.substring(2);
          valor.trim();
          long sats = valor.toInt();

          if (sats > 0) {
            current_sats = sats;
            Serial.println("[SERIAL] ANTES de createInvoice()");
            Serial.flush();

            createInvoice(current_sats);

            Serial.println("[SERIAL] DEPOIS de createInvoice()");
            Serial.flush();
          } else {
            Serial.println("[ERRO] Valor de sats invalido.");
          }
        }

        // C ou C: - verificar pagamento
        else if (
          serialInput.equalsIgnoreCase("C") ||
          serialInput.equalsIgnoreCase("C:")
        ) {
          Serial.println("[SERIAL] ANTES de manualCheck()");
          Serial.flush();

          manualCheck();

          Serial.println("[SERIAL] DEPOIS de manualCheck()");
          Serial.flush();
        }

        // L ou L: - consultar saldo
        else if (
          serialInput.equalsIgnoreCase("L") ||
          serialInput.equalsIgnoreCase("L:")
        ) {
          Serial.println("[SERIAL] ANTES de checkBalance()");
          Serial.flush();

          checkBalance();

          Serial.println("[SERIAL] DEPOIS de checkBalance()");
          Serial.flush();
        }

        // H - voltar para a tela inicial
        else if (serialInput.equalsIgnoreCase("H")) {
          Serial.println("[SERIAL] Voltando para tela inicial...");

          input_amount = "";
          current_payment_hash = "";
          current_bolt11 = "";

          desenharTelaTeclado();

          Serial.println("[SERIAL] Tela inicial exibida.");
        }

        // H: - mostrar informacoes uteis do sistema
        else if (serialInput.equalsIgnoreCase("H:")) {
          mostrarInformacoesSistema();
        }

        else {
          Serial.println("[SERIAL] Comando desconhecido.");
          Serial.println("Comandos: H, H:, I:1500, C, L");
        }

        serialInput = "";
      }
    }
    else if (c >= 32 && c <= 126) {
      if (serialInput.length() < 64) {
        serialInput += c;
      } else {
        Serial.println("[ERRO] Comando serial muito longo; descartado.");
        serialInput = "";
      }
    }
  }


  // =====================================================
  // 2. VERIFICAR TOUCH
  // =====================================================

  verificarTouch();


  // =====================================================
  // 3. DEBUG - CONFIRMAR LOOP ATIVO
  // =====================================================

  if (millis() - lastDebugPrintTime >= 30000) {
    lastDebugPrintTime = millis();
    Serial.println("[DEBUG] Loop ativo - Sistema rodando normalmente.");
  }


  // =====================================================
  // 4. VERIFICACAO AUTOMATICA DE PAGAMENTO
  // =====================================================

  if (
    currentScreen == SCREEN_QR_CODE &&
    current_payment_hash != ""
  ) {
    if (millis() - lastAutoCheckTime >= 3000) {
      lastAutoCheckTime = millis();
      verificarPagamentoSilencioso();
    }
  }


  // =====================================================
  // 5. RETORNO AUTOMATICO APOS PAGAMENTO
  // =====================================================

  if (currentScreen == SCREEN_PAID) {
    if (millis() - paidScreenTime >= 5000) {
      input_amount = "";
      current_payment_hash = "";
      current_bolt11 = "";

      desenharTelaTeclado();
    }
  }
}
