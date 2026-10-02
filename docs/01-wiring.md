# 01 — Esquema de ligação e alimentação

> ⚠️ **Tudo é 3.3 V.** CC1101 e NRF24L01+ **NÃO** são tolerantes a 5 V nos pinos de
> lógica. Ligar 5 V neles queima o chip. O ESP32-S3 também é 3.3 V — ligação direta, sem
> conversor de nível, é o correto.

## 1. Os dois barramentos SPI

| Barramento | Host | Quem usa | Velocidade |
|------------|------|----------|-----------|
| Display | SPI3 / HSPI | ILI9341 | 40 MHz |
| Rádios | SPI2 / FSPI (`SPI` global) | CC1101 + NRF24 #1 + NRF24 #2 | ~4–10 MHz |

No barramento dos rádios, **SCK/MOSI/MISO são compartilhados**; o que diferencia cada
módulo é o **CS/CSN** (e o **CE** dos NRF). Só um módulo é "selecionado" por vez.

## 2. Tabela mestre de pinos (fonte: `include/pins.h`)

### Display ILI9341 → SPI3/HSPI
| Pino do módulo | GPIO ESP32-S3 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| CS | 10 |
| RESET | 14 |
| DC / RS | 9 |
| SDI / MOSI | 11 |
| SCK | 12 |
| LED (backlight) | 21 |
| SDO / MISO | 13 |
| T_* (touch) | — não usado (navegação é pelo encoder) |

### Barramento dos rádios (compartilhado) → SPI2/FSPI
| Sinal | GPIO |
|---|---|
| SCK | 18 |
| MOSI | 17 |
| MISO | 8 |

### CC1101 (sub-GHz)
| Pino | GPIO |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SCK | 18 (compart.) |
| MOSI / SI | 17 (compart.) |
| MISO / SO | 8 (compart.) |
| CSN | 7 |
| GDO0 | 6 |
| GDO2 | — (não usado) |

### NRF24L01+PA+LNA #1 / #2
| Pino | NRF #1 | NRF #2 |
|---|---|---|
| VCC (**3V3 dedicado!**) | 3V3-RF | 3V3-RF |
| GND | GND | GND |
| SCK | 18 (compart.) | 18 (compart.) |
| MOSI | 17 (compart.) | 17 (compart.) |
| MISO | 8 (compart.) | 8 (compart.) |
| CE | 4 | 42 |
| CSN | 2 | 41 |
| IRQ | — | — |

### Encoder KY-040
| Pino | GPIO |
|---|---|
| + (VCC) | 3V3 |
| GND | GND |
| CLK (A) | 38 |
| DT (B) | 39 |
| SW (botão) | 47 |

### Botões (a outra perna vai ao GND; usam pull-up interno)
| Botão | GPIO |
|---|---|
| OK / Enter | 5 |
| Back / Sair | 1 |
| Aux | 40 |

### LEDs (ânodo → GPIO via resistor 220–330 Ω; cátodo → GND)
| LED | GPIO |
|---|---|
| Status (heartbeat) | 15 |
| Atividade RF | 16 |

## 3. Alimentação — a parte que mais dá problema

Os **NRF24 PA+LNA** puxam picos de corrente altos (>100 mA cada em RX, mais em TX). Se
você alimentá-los pelo pino 3V3 da DevKit, junto com o Wi-Fi do ESP32 ligado, o regulador
da placa cai de tensão (**brownout**) e o dispositivo reinicia ou os NRF "somem" do bus.

**Solução (faça assim):**

```
USB 5V (ou bateria) ──┬─► VIN/5V do ESP32-S3 DevKit ──► (LDO interno) ──► 3V3 do ESP32
                      │
                      └─► AMS1117-3.3 (ou MP1584) ──► trilha "3V3-RF" ──► CC1101 + 2x NRF24
                                                          │
                                       bulk 100–470 µF ───┤
```

- Use uma trilha **3V3-RF separada** (do regulador dedicado) para os três rádios.
- **GND é comum** a tudo (ESP32, regulador, todos os módulos) — junte todos os GND.
- **Capacitor de 10 µF** entre VCC e GND **bem perto** de cada NRF (e do CC1101).
- **100 nF cerâmico** em paralelo com cada 10 µF (filtra ruído de alta frequência).
- **100–470 µF** de bulk na entrada da trilha 3V3-RF.

> Alternativa pronta: existem "adaptadores de alimentação para NRF24" (placa quadrada com
> regulador + cap embutidos). Se tiver, use-os — resolvem o problema de uma vez.

## 4. Componentes passivos: o que vai onde

Resumo de **resistores, diodos e capacitores** — leia a coluna "Por quê" para entender,
não decore.

| Componente | Valor | Onde | Por quê |
|---|---|---|---|
| Resistor (LED) | **220–330 Ω** | em série com cada LED: `GPIO → R → ânodo` / `cátodo → GND` | limita a corrente (~5–10 mA); sem ele o LED/GPIO queima |
| Resistor (backlight) | ~100 Ω (opcional) | em série com o pino LED do display, **se** o módulo não tiver | limita a corrente do backlight |
| Resistor (botões) | — | **não precisa**: usamos `INPUT_PULLUP` interno | pull-up já vem do ESP32 |
| Divisor p/ bateria | **2× 100 kΩ** (opcional) | `BAT+ → R → (pino ADC1) → R → GND` | ler a tensão da LiPo num pino ADC (ver §6) |
| **Cerâmico** "104" | **100 nF** | VCC↔GND **coladinho** em CADA módulo (CC1101, NRF#1, NRF#2, display) e perto do 3V3 do ESP | filtra ruído de **alta frequência** |
| **Eletrolítico** pequeno | **10 µF** | VCC↔GND em **cada NRF** (e no CC1101) | reservatório local para os **picos de TX** |
| **Eletrolítico** bulk | **100–470 µF** | na **entrada de energia** e na saída da **trilha 3V3-RF** | segura quedas em grandes degraus de carga (Wi-Fi/PA) |
| **Diodo Schottky** | 1N5817 / SS34 | em série na entrada de energia (ver abaixo) | proteção contra **inversão de polaridade** |
| Diodo flyback | 1N4148 / 1N4007 | em paralelo, **se** adicionar buzzer/motor | protege o GPIO de picos indutivos |

### Cerâmico × eletrolítico (regra prática)
- **Cerâmico 100 nF** = filtro "rápido" → sempre o **mais perto possível** dos pinos
  VCC/GND do chip. Não tem polaridade (tanto faz o lado).
- **Eletrolítico 10 µF / bulk** = "tanque" de energia → segura os picos. O de 10 µF do
  NRF deve ficar **bem junto**; o bulk pode ficar na entrada da trilha.
- O combo clássico é os **dois em paralelo** no mesmo VCC/GND:

```
   VCC do módulo ──┬─────────┬──
                   │         │
                 100nF     10µF(+)        <- ceramico (rapido) + eletrolitico (tanque)
                   │         │
   GND do módulo ──┴─────────┴──          (faixa "-" do eletrolitico vem para o GND)
```

### ⚠️ Polaridade — confira ANTES de soldar
- **Eletrolítico:** a perna **curta** e a **faixa clara com "−"** são o **negativo → GND**.
  Invertido, ele esquenta, estufa e pode **estourar**.
- **Cerâmico (104):** **sem** polaridade.
- **Diodo:** a **faixa (anel)** marca o **cátodo**. A corrente flui do lado **sem faixa
  (ânodo) → faixa (cátodo)**. LED também é diodo: perna longa = ânodo (+).

### Diodo de proteção / bateria
Só USB? Um **único Schottky em série** na entrada já protege contra ligar invertido:

```
   Fonte + ──|>|── VIN/entrada do regulador      (faixa do diodo aponta p/ o circuito)
```

USB **e** bateria LiPo? Dois Schottky fazem o chaveamento automático (a fonte de maior
tensão "vence"; ideal usar um módulo TP4056 com load-sharing):

```
   USB 5V  ──|>|──┐
                  ├──► entrada do regulador / VIN
   LiPo +  ──|>|──┘
```

## 5. Display — VCC e backlight

- Muitos módulos ILI9341 "vermelhos" têm regulador e aceitam **5 V** no VCC, com a lógica
  já em 3.3 V. Outros são 3.3 V puro. Na dúvida, **alimente VCC com 3.3 V** e mantenha a
  lógica em 3.3 V (é sempre seguro).
- O pino **LED** (backlight) normalmente já tem resistor no módulo; ligá-lo no GPIO 21
  permite ligar/desligar e até PWM de brilho. Se o brilho ficar fraco/forte demais ou o
  GPIO esquentar, coloque um resistor de ~100 Ω em série (ou um transistor NPN se a
  corrente for alta).

## 6. Monitor de bateria (opcional)

Quer ver a carga da LiPo na tela? Faça um **divisor com 2× 100 kΩ** entre `BAT+` e `GND`,
ligando o meio a um pino **ADC1 (GPIO 1–10)** — ADC1 é obrigatório porque o ADC2 conflita
com o Wi-Fi. Como quase todos os GPIO 1–10 já estão em uso, libere um (ex.: o `DISP_MISO`
no GPIO 13 não é necessário sem touch — mas 13 é ADC2; prefira reaproveitar um botão ou o
GPIO de um LED para o ADC). Some um **100 nF** do pino ao GND para estabilizar a leitura.

## 7. Regras de ouro

- 🧲➖ **Polaridade:** eletrolíticos e diodos têm lado certo — confira a **faixa** antes de
  soldar (eletrolítico: faixa = negativo → GND; diodo: faixa = cátodo, lado da saída).
- 🔌 **Antenas primeiro:** rosqueie as antenas nos NRF **antes** de energizar. Ligar o PA
  sem antena pode danificar o estágio de potência.
- ⚡ **Confirme 3V3, nunca 5V** nos pinos de CC1101/NRF antes de ligar (use o multímetro).
- 🧲 **Afaste as duas antenas NRF** entre si e do ESP32, e mantenha os fios SPI curtos.
- 🌍 **GND único:** todos os terras conectados, senão a SPI lê lixo.
