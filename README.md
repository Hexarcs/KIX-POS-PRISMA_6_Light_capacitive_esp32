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
