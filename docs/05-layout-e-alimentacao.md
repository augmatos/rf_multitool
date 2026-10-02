# 05 — Layout na placa furada + alimentação (montagem passo a passo)

> Complementa o [01-wiring.md](01-wiring.md) (esquema elétrico) e **não substitui** o
> [03-soldering.md](03-soldering.md). Aqui o foco é **onde cada peça vai na matriz** e
> **em que ordem soldar e testar**, já com a alimentação decidida.

## Decisões que valem para tudo

- **Alimentação (a que você escolheu, e que o [01-wiring.md §3](01-wiring.md) já prescreve):**
  chega **5 V + GND** de uma placa externa. Esse 5 V alimenta o ESP pelo **VIN** (o regulador
  interno dele faz o 3V3 da lógica) **e** a entrada de um **LDO dedicado**, que gera uma barra
  **`3V3-RF` separada** só para CC1101 + 2× NRF24. As duas redes de 3V3 nunca se encostam.
- **Placa:** matriz **35 colunas × 12 linhas (A–L)**, passo 2,54 mm.
- **O ESP é grande:** com os dois headers ele ocupa **~22 furos × ~10** e toma ~2/3 da placa.
  Por isso **display, encoder, botões e LEDs ficam FORA da placa** (fios voadores / dupont).
- **Roteamento é pela face de baixo:** o corpo do ESP cobre furos por cima, mas os jumpers
  passam por baixo, entre os pads do soquete. Nada de soldar módulo direto — **tudo em
  soquete fêmea**.
- **Orientação assumida nos furos abaixo:** borda do **USB na coluna 1**, **antena na coluna 22**.
  Se você montar o módulo espelhado, **espelhe as colunas** das tabelas.

---

## Figura 1 — A placa inteira (35 × 12)

```
      col 1                                   col 22 | 24                      col 35
        v                                        v   |  v                        v
  A   (*)===================== GND (linha A) =========================================(*)
  B   |5V|--------------- header J1 (topo) ----------|3V3|   .  LDO   .  bulk  .  NRF1 (^)
  C   |  +------------------------------------------+   |   .  CC1101 .  .  .  |3V3-RF|
  D   |  |                                          |   |   .  .  .  .  .  .  .  .  .
  E   |  |          ESP32-S3 DevKitC-1              |   |   .  .  .  .  .  .  .  .  .
  F   |  |         (em soquete femea)               |   |   .  [ ZONA RF + LDO ]  .  .
  G   |  |   corpo cobre col 3-21; rotas por baixo  |   |   .  .  .  .  .  .  .  .  .
  H   |  |                                          |   |   .  .  .  .  .  .  .  .  .
  I   |  |                                          |   |   .  .  .  .  .  .  .  .  .
  J   |  +------------------------------------------+   |   .  .  .  .  .  .  .  .  .
  K   |  |--------------- header J3 (base) ----------|   |   .  .  .  .  .  .  NRF2 (v)
  L   (*)===================== GND (linha L) =========================================(*)
      ^USB (borda esq.)                        antena^   ^entrada 5V+GND (canto col 1)
```

- **Linhas A e L = barras de GND** (fio nu atravessando), ligadas entre si num ponto → GND estrela.
- **Colunas 1–22 = ESP** (pinos J1 na linha B, J3 na linha K).
- **Colunas 24–35 = zona RF** (LDO + CC1101 + 2× NRF) — detalhe na Figura 2.
- **Entrada 5V + GND** chega no **canto da coluna 1** (perto do pino VIN e do USB).

---

## Figura 2 — Zoom da zona RF + alimentação (colunas 23–35)

```
       c23   c24   c25   c26   c27   c28/29    c31/32        (borda->)
        |     |     |     |     |     |          |
  A  ===================== GND (linha A) =============================
  B         [bulk+]                         G o---o V   NRF24 #1   (^) antena p/ fora
  C   +----+  |     ||3V3-RF|          CC   CE o---o CSN    10u+104
  D   |LDO |  |     ||      |          1101  CK o---o MOSI
  E   |IN  o--+     ||      |         [V G   MI o---o IRQ
  F   |GND o--(GND) ||      |          CSN
  G   |OUT o========||      |          GDO0
  H   +----+        ||      |          SPI]        G o---o V   NRF24 #2  (v) antena
  I                 ||      |                       CE o---o CSN   10u+104
  J     [bulk+]-----||      |                       CK o---o MOSI
  K                 ||3V3-RF|                       MI o---o IRQ
  L  ===================== GND (linha L) =============================
        \____ barra 3V3-RF (fio nu vertical, col 26) alimenta so os radios ____/
```

Legenda dos pinos NRF24 (padrão de todo módulo `+PA+LNA`):

```
   G  = GND        V  = VCC (na barra 3V3-RF)
   CE = Chip Enable   CS = CSN (chip select)
   CK = SCK        MO = MOSI      MI = MISO      IRQ = nao usado
```

- **LDO** (col 23–24): `IN` recebe o 5 V, `GND` na barra, `OUT` = barra **`3V3-RF`** (col 26).
- **CC1101** (col 28–29): confira a **ordem no silk do seu módulo**; o que importa é
  `VCC → 3V3-RF`, `GND`, os 3 fios do bus (SCK/MOSI/MISO), `CSN` e `GDO0`.
- **NRF #1 em cima, NRF #2 embaixo**, antenas para **bordas opostas**, penduradas para fora.
- **10 µF + 100 nF** colados no `VCC` de **cada** rádio (marcados `10u+104`).
- **Bus SPI em cadeia:** `SCK·MOSI·MISO` saem do ESP, passam no CC1101 e seguem para NRF1 e NRF2.

---

## Tabela 1 — Pinos do ESP: sinal → GPIO → furo

Linha **B** = header J1 (topo) · linha **K** = header J3 (base).

| Grupo | Sinal (GPIO) | Furo | Sinal (GPIO) | Furo |
|---|---|---|---|---|
| **Energia** | 5V / VIN | col 2, B | 3V3 saída | col 21–22, B |
| | GND | col 1, B · col 1/2/22, K | (barras) | linhas A e L |
| **Bus rádios (SPI2)** | MISO (8) | col 11, B | SCK (18) | col 12, B |
| | MOSI (17) | col 13, B | — | — |
| **CC1101** | CS (7) | col 16, B | GDO0 (6) | col 17, B |
| **NRF24 #1** | CE (4) | col 19, B | CSN (2) | col 18, K |
| **NRF24 #2** | CE (42) | col 17, K | CSN (41) | col 16, K |
| **Display (SPI3)** | CS10 / DC9 / RST14 | col 7 / 8 / 3, B | MOSI11 / SCK12 / MISO13 | col 6 / 5 / 4, B |
| | Backlight (21) | col 5, K | — | — |
| **Encoder** | CLK (38) | col 13, K | DT (39) / SW (47) | col 14 / col 6, K |
| **Botões** | OK (5) | col 18, B | Back (1) / Aux (40) | col 19 / col 15, K |
| **LEDs** | STAT (15) | col 15, B | RF (16) | col 14, B |

> Confirma o desenho: **bus + CC1101 + NRF1-CE** ficam no lado B (topo); só **NRF-CSN/CE +
> encoder** descem para o lado K. O clock nunca cruza — fica curto, em cadeia, no lado B.

---

## Tabela 2 — Passivos: onde cada um vai (e polaridade)

| Componente | Valor | Entre (de → para) | Polaridade / obs |
|---|---|---|---|
| **Diodo Schottky** | 1N5817 / SS34 | 5V da entrada (ânodo) → nó 5V da placa (cátodo) | **faixa aponta p/ dentro** do circuito |
| Cap bulk entrada | 100–470 µF | nó 5V → GND | eletrolítico: **perna curta / faixa "−" = GND** |
| Cap bulk 3V3-RF | 100–470 µF | saída do LDO → GND | idem |
| Cap 10 µF **(×3)** | 10 µF | VCC de **cada** rádio → GND, colado no módulo | eletrolítico, respeite o "+" |
| Cap 100 nF **(×4–5)** | "104" | VCC↔GND de cada módulo (CC1101, 2× NRF) e no LDO | cerâmico, **sem** polaridade |
| Resistor LED **(×2)** | 220–330 Ω | GPIO15 e GPIO16 → ânodo do LED | LED **cátodo → GND** |
| Resistor backlight | ~100 Ω (opc) | GPIO21 → pino LED do display | só se o módulo não tiver |
| Divisor bateria | 2× 100 kΩ (opc) | BAT+ → (pino ADC1) → GND | + 100 nF do meio p/ GND |

> **10 µF + 100 nF em paralelo** no VCC de cada rádio é o detalhe que segura os picos de TX do
> PA+LNA e evita o NRF "sumir" do barramento.

---

## Tabela 3 — Fios voadores para a interface (fora da placa)

| Peça (no painel) | Fio → pino ESP | Alimentação | Resistor |
|---|---|---|---|
| Display CS·DC·RST·MOSI·SCK·MISO | 10·9·14·11·12·13 | VCC = 3V3 lógico · GND | — |
| Display backlight | 21 | brilho puxa do 5V | ~100 Ω opc |
| Encoder CLK·DT·SW | 38·39·47 | + = 3V3 · GND | pull-up interno (sem R) |
| Botões OK·Back·Aux | 5·1·40 | outra perna → GND | pull-up interno (sem R) |
| LED status / RF | 15 / 16 (**via resistor na placa**) | cátodo → GND | 220–330 Ω on-board |

> **Onde essas peças ficam fisicamente:** o **visor, o encoder e os botões NÃO vão na placa
> furada** — vão na **tampa/painel da caixa** (você precisa ver o visor e apertar/girar os
> controles). A placa fica **atrás** deles, dentro da caixa, e cada peça se liga por **fios
> voadores (dupont)** aos furos do soquete do ESP da Tabela 1. O visor puxa `VCC` do **3V3
> lógico** (pino 3V3 do ESP), `GND` da barra, e o backlight do **5V**. Dica: solde uma
> **régua de pino-macho** numa borda livre como conector e plugue os dupont ali — fica
> desmontável.

---

## Caminho da energia (resumo)

1. **5V + GND** externos entram no **canto da coluna 1** → **Schottky** em série no 5V → **bulk** p/ GND.
2. Do nó 5V saem dois ramos: → **pino 5V do ESP (col 2, B)** e → **entrada do LDO** (col 23–24).
3. **LDO → barra `3V3-RF`** (fio nu vertical, col 26) alimenta **só** CC1101 + 2× NRF, cada um
   com seus 10 µF + 100 nF.
4. **3V3 do ESP (col 21–22)** vai por fio voador para display / encoder / botões.
5. **GND é estrela:** junte as linhas A e L e todos os retornos num ponto perto da entrada (col 1).

---

## GND estrela — por quê e como (leia antes de soldar terra)

**Existe um único GND no aparelho inteiro.** Não é "o GND do ESP" *ou* "o GND do LDO": o GND
da entrada, o do ESP, o do LDO e o de **cada** módulo são todos **o mesmo net**, ligados no
mesmo ponto. O que é separado são só os **positivos** — `5V`, `3V3-lógico` e `3V3-RF`. O
**GND é compartilhado por todos**.

**Por que importa:** a SPI decide se um bit é 0 ou 1 comparando a tensão do sinal com o GND —
o GND é o "zero" de referência. Fio tem resistência; quando o ESP (Wi-Fi/PA) dá um pico de
corrente, essa corrente cria uma pequena queda de tensão no fio de terra (*ground bounce*). Se
o GND de um NRF "flutua" em relação ao do ESP, um "1" pode virar "0" → **dado corrompido, o
módulo "some" do barramento, trava**.

- ❌ **Errado (em série):** ESP → NRF1 → NRF2 → CC1101, um puxando o GND do outro. Cada trecho
  acumula queda; o último módulo fica com o pior "zero".
- ✅ **Certo (estrela):** cada GND vai **direto** para o ponto comum (as barras GND das linhas
  A/L, unidas perto da entrada). Todos enxergam o mesmo "zero".

**Na prática:** use **fio grosso / barra larga** nas linhas A e L — não deixe o GND depender de
uma trilha fininha e comprida. **Terra gordo e curto = SPI limpa.**

---

## Passo a passo: o que soldar primeiro e o que testar

> Regra geral: **energia antes de sinal, um bloco por vez, e mede/roda o teste antes de seguir.**
> Ambientes de teste (do [platformio.ini](../platformio.ini)):
> `test_wifi`, `test_display`, `test_encoder`, `test_cc1101`, `test_nrf24`.
> Comando: `pio run -e <ambiente> -t upload -t monitor`.

### Passo 0 — Soquetes e conferência da placa
- **Solde os soquetes fêmea** do ESP e dos 3 rádios (nada de módulo direto). Deixe os módulos **fora** por enquanto.
- **Teste:** multímetro em continuidade — confira que furos que **não** deveriam se tocar estão isolados.

### Passo 1 — Terra e entrada de 5 V (sem módulos, sem ESP)
- Solde as **barras GND** (linhas A e L) e junte-as num **ponto estrela** perto da col 1.
- Solde a **entrada 5V+GND**, o **Schottky** em série no 5V (faixa p/ dentro) e o **bulk de entrada**.
- **Teste:** aplique 5 V na entrada. Meça:
  - **5V ↔ GND: sem curto** (resistência não pode ser ~0 Ω).
  - Tensão no **nó 5V** ≈ 5 V (com a queda do Schottky, ~4,6–4,8 V — normal).
  - **Confira a polaridade** do bulk antes de energizar. ❌ Se esquentar/estufar, desligue já.

### Passo 2 — LDO e barra 3V3-RF (ainda sem rádios)
- Solde o **LDO**, o **bulk do 3V3-RF** e puxe a **barra `3V3-RF`** (col 26).
- **Teste:** com 5 V aplicado, meça a barra **`3V3-RF` = 3,3 V** (±0,1). Meça também **3,3 V** em
  cada furo `VCC` dos soquetes dos rádios. ⚠️ **Só siga se ler 3,3 V — nunca 5 V — nos rádios.**

### Passo 3 — Encaixar o ESP e dar vida
- Ligue o **5 V do ESP (col 2, B)** ao nó 5V; encaixe o ESP no soquete.
- **Teste:** grave o teste que **não precisa de fiação**:
  `pio run -e test_wifi -t upload -t monitor`.
  Ele deve bootar, listar redes e **não** entrar em brownout/reboot. No firmware principal, o app
  **Uptime & Sistema** mostra heap e temperatura — bom sinal de alimentação estável.

### Passo 4 — Decoupling dos rádios
- Solde **10 µF + 100 nF** no `VCC/GND` de **cada** soquete de rádio (o mais colado possível).
- **Teste:** visual + continuidade (100 nF sem polaridade; 10 µF com o "+" certo).

### Passo 5 — Display (fios voadores)
- Ligue os 6 fios do display (Tabela 3) + **backlight (21)** + `VCC = 3V3 lógico` + `GND`.
- **Teste:** `pio run -e test_display -t upload -t monitor` → tem que **desenhar na tela**.
  Cores/lixo na tela? troque `-DILI9341_2_DRIVER` por `-DILI9341_DRIVER` (ver [platformio.ini](../platformio.ini)).

### Passo 6 — Encoder, botões e LEDs
- Ligue **CLK/DT/SW**, os 3 botões (outra perna → GND) e os 2 LEDs **com resistor 220–330 Ω** em série.
- **Teste:** `pio run -e test_encoder -t upload -t monitor` → girar/clicar tem que contar certo.
  Encoder "pulando"? no seu clone o GPIO38 pode ser o LED RGB — mova `ENC_CLK` p/ GPIO19 (ver [pins.h](../include/pins.h)).

### Passo 7 — CC1101
- **Rosqueie a antena** antes de energizar. Ligue `VCC → 3V3-RF`, `GND`, o **bus** (SCK/MOSI/MISO),
  `CSN (7)` e `GDO0 (6)`.
- **Teste:** `pio run -e test_cc1101 -t upload -t monitor` → tem que **detectar o chip** (version/part number).
  Não detecta? confira o silk do módulo (ordem dos pinos), o CSN e o GND comum.

### Passo 8 — NRF24 #1
- **Rosqueie a antena.** Ligue o **bus em cadeia** (a partir do CC1101), `CE (4)`, `CSN (2)`,
  `VCC → 3V3-RF`, `GND`, e confirme os **10 µF + 100 nF**.
- **Teste:** `pio run -e test_nrf24 -t upload -t monitor` → o #1 tem que responder (`isChipConnected`/registradores coerentes).

### Passo 9 — NRF24 #2
- Igual ao #1, com `CE (42)` e `CSN (41)`; **antena na borda oposta** ao #1.
- **Teste:** rode o `test_nrf24` de novo apontando para o #2 (ou o **Diagnóstico** do firmware).

### Passo 10 — Firmware completo + self-test
- **Teste:** `pio run -e main -t upload -t monitor`. Abra o app **Diagnóstico** — ele detecta
  **CC1101 / NRF #1 / NRF #2** de uma vez. Se os três aparecem, o hardware está fechado. ✅

---

## Regras de ouro (repetição proposital)

- 🔌 **Antena antes de energizar** o CC1101 e os NRF — ligar o PA sem antena danifica o estágio.
- ⚡ **Meça 3,3 V, nunca 5 V**, no VCC dos rádios antes de encaixá-los.
- 🌍 **GND único (estrela)** — sem isso a SPI lê lixo.
- 🧲 **Antenas dos 2 NRF afastadas** (bordas opostas) e **fios SPI curtos**.
- 🧲➖ **Polaridade:** eletrolítico (faixa "−" = GND) e diodo (faixa = cátodo, lado da saída) —
  confira **antes** de soldar. Mais detalhes em [01-wiring.md §4](01-wiring.md).
