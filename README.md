# KIX POS

Ponto de venda (PDV) **Lightning Network** para **ESP32** com tela TFT 240×320,
touch capacitivo **GT911** e integração com **LNbits**.

- `pos_kix_capacitivo_tamc_gt911.ino` — firmware principal
- [`architecture.md`](architecture.md) — arquitetura detalhada (EN)
- [`arquitetura.md`](arquitetura.md) — arquitetura detalhada (PT-BR)

---

## Início rápido

### 1. O que você precisa

- Placa **ESP32** com tela TFT 240×320 (portrait) e touch **GT911**
  (ex.: *ESP32-2432S028 / Cheap Yellow Display*)
- **Arduino IDE** (ou PlatformIO) com as bibliotecas abaixo instaladas
- Uma carteira/instância **LNbits** com chave de API (`X-Api-Key`)

### 2. Configurar credenciais

Edite o topo de `pos_kix_capacitivo_tamc_gt911.ino`:

```cpp
const char* ssid     = "SEU_WIFI";
const char* password = "SUA_SENHA";

String lnbits_url         = "https://SEU-LNBITS/api/v1/payments";
String lnbits_wallet_url  = "https://SEU-LNBITS/api/v1/wallet";
String lnbits_api_key     = "SUA_CHAVE_X-API-KEY";
```

> ⚠️ Não versione a chave real. Rote-a se já foi compartilhada.

### 3. Configurar o driver da tela

`TFT_eSPI` exige o `User_Setup.h` correto para a sua placa. Para o CYD
(ESP32-2432S028), use a definição de `ILI9341` 240×320 com os pinos SPI
apropriados. O firmware assume **rotação `2`** (retrato).

### 4. Compilar e gravar

1. Selecione a placa **ESP32 Dev Module** (ou a variante do seu hardware).
2. Configure **baud 115200** no Monitor Serial.
3. Compile e grave (`Ctrl+U` na Arduino IDE).

### 5. Usar

- **Na tela**: digite o valor em sats → `OK` → exiba o QR → cliente paga →
  "PAGO COM SUCESSO!".
- **Pelo Serial** (Monitor Serial, 115200):

| Comando     | Ação                                  |
|-------------|---------------------------------------|
| `I:1500`    | Cria invoice de 1500 sats             |
| `C`         | Verifica pagamento da invoice ativa   |
| `L`         | Consulta saldo da carteira            |
| `H`         | Volta à tela inicial                  |
| `H:`        | Mostra status do sistema              |

---

## Compilar e gravar via Arduino CLI

```bash
# =====================================================================
# AVISO: Estes comandos DEVEM ser executados de dentro da pasta do projeto 
# (ex: ~/kix-pos)
# onde estão o arquivo .ino e a pasta ./libraries. O uso do ponto (.) 
# indica o diretório atual de trabalho.
# =====================================================================

# === COMPILAR ===
arduino-cli compile \
  --fqbn esp32:esp32:esp32:PartitionScheme=huge_app \
  --libraries ./libraries \
  --jobs 4 \
  --build-property compiler.c.extra_flags="-Wno-error=implicit-function-declaration -Wno-error=implicit-int -Wno-implicit-function-declaration" \
  --verbose \
  .

# === UPLOAD (GRAVAR NA PLACA) ===
arduino-cli upload \
  -p /dev/ttyUSB0 \
  --fqbn esp32:esp32:esp32:PartitionScheme=huge_app \
  .

# === COMPILAR E GRAVAR EM UMA SÓ LINHA ===
arduino-cli compile \
  --fqbn esp32:esp32:esp32:PartitionScheme=huge_app \
  --libraries ./libraries \
  --jobs 4 \
  --build-property compiler.c.extra_flags="-Wno-error=implicit-function-declaration -Wno-error=implicit-int -Wno-implicit-function-declaration" \
  --verbose \
  . \
&& \
arduino-cli upload \
  -p /dev/ttyUSB0 \
  --fqbn esp32:esp32:esp32:PartitionScheme=huge_app \
  .

# === MONITOR SERIAL ===
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200

# === EXPORTAR BINÁRIO ===
arduino-cli compile \
  --verbose \
  --fqbn esp32:esp32:esp32:PartitionScheme=huge_app \
  --libraries ./libraries \
  --jobs 4 \
  --build-property compiler.c.extra_flags="-Wno-error=implicit-function-declaration -Wno-error=implicit-int -Wno-implicit-function-declaration" \
  --output-dir ./build_output \
  .
```

> Os mesmos comandos estão em [`comandos_arduino_cli_upload.txt`](comandos_arduino_cli_upload.txt).

---

## Como funciona (resumo)

```
  teclado (sats) ──► createInvoice() ──► POST /payments
                                           │
                                           ▼
                                     QR Code (BOLT11)
                                           │
        cliente escaneia e paga ◄──────────┘
                                           │
        loop(): poll a cada 3 s ──► GET /payments/{hash}
                                           │
                                     paid == true
                                           │
                                           ▼
                                   "PAGO COM SUCESSO!"
```

Detalhes completos (diagramas ASCII, máquina de estados, referência de funções e
pontos de segurança) em [`arquitetura.md`](arquitetura.md) ou
[`architecture.md`](architecture.md).
