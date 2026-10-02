# rf_multitool — dispositivo de pentest RF (ESP32-S3)

Projeto **novo e independente**: um dispositivo portátil estilo "Flipper", com display,
encoder e três rádios — **CC1101** (sub-GHz), **2× NRF24L01+PA+LNA** (2.4 GHz) e o
**Wi-Fi/BLE** interno do ESP32-S3.

> ⚠️ **Uso responsável.** Este dispositivo recebe e transmite sinais de rádio. Use
> **somente** em redes e equipamentos que você **possui** ou para os quais tem
> **autorização explícita** (contrato de pentest, laboratório, CTF). Transmitir/capturar
> em faixas licenciadas sem autorização pode ser ilegal.
>
> 🔴 Os apps **Rogue AP** e **Deauth (PENTEST!)** fazem parte de um módulo ofensivo (ver
> [docs/04-funcoes-e-ideias.md](docs/04-funcoes-e-ideias.md)) e exigem confirmação
> explícita **toda vez** que abrem. Use apenas em engajamentos com **autorização por
> escrito** — nunca contra terceiros sem consentimento. O Deauth também tem rajada fixa
> (40 frames) + cooldown de 5s **fixos no código**, não configuráveis pela interface.

## Relação com o projeto ESPhishing

Este é um projeto **separado** do ESPhishing (que continua em
`..\esphishing_esp32s3\esphishing`, intocado). A meta de longo prazo é integrar o portal
Wi-Fi do ESPhishing como **um item do menu** deste multitool — passo futuro, quando você
quiser. Por ora, o foco é montar e validar o hardware com o firmware daqui.

## Hardware (BOM)

**Você já tem:** ESP32-S3-DevKitC-1, 1× CC1101, 2× NRF24L01+PA+LNA (+antenas),
display ILI9341 2.4", encoder KY-040, botões táteis, LEDs, resistores, placa
universal 10×10 cm.

**Precisa providenciar:**
- 1× **regulador 3V3 dedicado para os NRF** — AMS1117-3.3 (módulo) ou step-down MP1584.
  O LDO da placa não dá conta dos NRF PA+LNA + Wi-Fi simultâneos.
- Capacitores **eletrolíticos**: **2–3× 10 µF** (no VCC de cada NRF e do CC1101) +
  **1× 100–470 µF** (bulk na entrada/3V3-RF). Capacitores **cerâmicos**: **vários 100 nF**
  (decoupling, um por módulo).
- **Diodo Schottky** 1N5817/SS34 (proteção de polaridade na entrada; 2× se fizer power-path
  USB+LiPo).
- **Barras de pino fêmea** (encaixe o ESP32 e TODOS os módulos — não solde os módulos
  direto na placa!).
- Resistores **220–330 Ω** (LEDs). Botões e KY-040 usam pull-up interno.
  (Opcional: 2× 100 kΩ para monitor de bateria.)
- Fio rígido fino (AWG 28–30) ou jumpers, solda, fluxo.
- Opcional: chave liga/desliga, bateria LiPo + carregador TP4056 para uso portátil.

> 📍 **Onde cada passivo vai (e polaridade):** ver [docs/01-wiring.md §4](docs/01-wiring.md).

## Arquitetura SPI (decisão central do projeto)

São 4 dispositivos SPI. Usamos **dois barramentos separados** para não brigarem:

```
ESP32-S3
 ├─ SPI3 / HSPI  ──► ILI9341 (display)             [rápido, 40 MHz]
 └─ SPI2 / FSPI  ──► CC1101 + NRF24#1 + NRF24#2     [compartilhado; cada um com seu CS]
```

O display é forçado para o HSPI pela flag `-DUSE_HSPI_PORT=1` (no `platformio.ini`).
Os rádios usam o objeto `SPI` global, dividindo SCK/MOSI/MISO e cada um com seu CSN/CE.

## Estrutura do projeto

```
platformio.ini        # build do multitool + 5 ambientes de teste
include/
  pins.h              # MAPA DE PINOS (fonte da verdade)
  config.h            # frequências, cores, flags
src/
  main.cpp            # firmware do multitool (menu + apps)
  tamagotchi.h/.cpp   # app RF Tamagotchi (bichinho que "come" sinais, persiste em NVS)
  beacon_spam.h/.cpp  # app Beacon Spammer (redes fantasma com SSIDs engraçados)
  beacon_spoof.h/.cpp # app Beacon Spoofing (clona SSID+BSSID de AP real)
  radar.h/.cpp        # app Radar de Pessoas (contador passivo Wi-Fi + BLE)
  subghz_capture.h/.cpp # app Sub-GHz Capturar/Repetir (grava/reproduz controle de código fixo)
  achievements.h/.cpp # app Achievements (troféus em NVS, dá XP bônus pro Tamagotchi)
  rogue_ap.h/.cpp     # app Rogue AP/Evil Twin — SÓ PENTEST AUTORIZADO, ver aviso no arquivo
  deauth_tool.h/.cpp  # app Deauth (DoS) — SÓ PENTEST AUTORIZADO, rajada limitada + cooldown
  tests/              # 1 sketch isolado por módulo
docs/
  01-wiring.md        # esquema de ligação + alimentação
  02-bench-testing.md # passo a passo: testar ANTES de soldar
  03-soldering.md     # passo a passo: soldar na placa 10×10
  05-layout-e-alimentacao.md # layout na matriz 35×12 + montar/testar por etapas
```

## Início rápido

1. Instale o [PlatformIO](https://platformio.org/install) (extensão do VS Code).
2. Abra esta pasta no VS Code; o PlatformIO baixa as libs sozinho.
3. Conecte o ESP32-S3 pela porta **USB** (a nativa) e rode o teste do Wi-Fi (sem fiação):
   ```
   pio run -e test_wifi -t upload -t monitor
   ```
4. Siga o **[guia de testes](docs/02-bench-testing.md)** para validar cada módulo na
   protoboard, depois o **[guia de soldagem](docs/03-soldering.md)**.
5. Firmware do multitool: `pio run -e main -t upload -t monitor`.

📚 Guias: [01 — Ligação](docs/01-wiring.md) · [02 — Testes](docs/02-bench-testing.md) ·
[03 — Soldagem](docs/03-soldering.md) · [04 — Funções e ideias](docs/04-funcoes-e-ideias.md) ·
[05 — Layout e alimentação](docs/05-layout-e-alimentacao.md)

## Apps do multitool (v0.1)

| App | Módulo | O que faz |
|-----|--------|-----------|
| WiFi Scanner | ESP32-S3 (interno) | Lista redes 2.4GHz: SSID, RSSI, canal, aberta/segura |
| 2.4GHz Analyzer | NRF24 #1 | Ocupação dos 80 canais (2400–2479 MHz) |
| Sub-GHz Sweep | CC1101 | Varre uma faixa e mostra o pico de RSSI |
| Sub-GHz Monitor | CC1101 | RSSI ao vivo numa frequência (ajuste pelo encoder) |
| 🔓 **Sub-GHz Capturar/Repetir** | CC1101 | grava e reproduz o SEU controle de código fixo (portão/tomada estilo PT2262/EV1527); NÃO funciona com rolling code |
| 🌈 **Waterfall Display** | CC1101 + NRF24 | cascata de RSSI com scroll de verdade (buffer em RAM); AUX troca entre sub-GHz e 2.4GHz |
| 🔵 **Scanner BLE** | BLE interno | lista dispositivos BLE por perto: nome, RSSI, fabricante (Apple reconhecido) |
| 📡 **Radar de Pessoas** | Wi-Fi + BLE | contador passivo de dispositivos (probe requests + anúncios BLE), gauge de "lotação" |
| 🐣 **RF Tamagotchi** | WiFi + NRF24 | bichinho que "come" os sinais do ambiente, evolui e persiste na flash |
| 🎭 **Beacon Spammer** | WiFi | redes fantasma com SSIDs engraçados; modos Engraçado/História + **Stress-test** limitado (pentest: dead-man, teto de SSIDs, cooldown, autorização) |
| 🔴 **Rogue AP (PENTEST!)** | WiFi | ⚠️ SÓ pentest autorizado — clona SSID/canal de rede já vista num scan, mede associação automática; sem portal cativo/captura de credenciais |
| 🔴 **Deauth (PENTEST!)** | WiFi | ⚠️ SÓ pentest autorizado — deauth broadcast num AP escolhido de um scan; rajada fixa de 40 frames + cooldown de 5s (não configurável, decisão de engenharia) |
| Diagnóstico | todos | Self-test: detecta CC1101 / NRF #1 / NRF #2 |
| 📟 **Uptime & Sistema** | ESP32-S3 (interno) | temperatura interna, heap livre (atual/mínimo), chip/flash, uptime — sem hardware extra |
| 🏆 **Achievements** | todos (agrega) | 11 troféus em NVS (redes, ferramentas usadas, uptime, evolução do Tamagotchi); dá XP bônus pro bichinho |

## Ideias de funções (o que dá pra adicionar)

Catálogo completo em **[docs/04-funcoes-e-ideias.md](docs/04-funcoes-e-ideias.md)**.
Já implementado: 📡 **Radar de Pessoas** · 🐣 **RF Tamagotchi** · 🎭 **Beacon Spammer** ·
🔓 **Sub-GHz Capturar/Repetir** · 🌈 **Waterfall Display** · 🔵 **Scanner BLE** ·
📟 **Uptime & Sistema** · 🏆 **Achievements** · 🔴 **Rogue AP/Evil Twin** · 🔴 **Deauth**
(ver tabela de apps acima). Próximos destaques:

- 📡 **Detector de deauth** e **detector de tracker/AirTag** (defensivos — diferentes do
  Deauth ofensivo: aqui é escutar se ALGUÉM MAIS está atacando por perto)
- 📻 **Decodificar sensores 433 MHz** da vizinhança · **fox hunt** (caça ao transmissor)
- 📊 **Wi-Fi Signal Logger** (RSSI médio/mín/máx)
- 🎮 joguinhos no encoder (Snake/Pong) · somar **BLE** como comida do Tamagotchi
- 🧰 **QR Code de Wi-Fi** na tela · relógio/clima · monitor de bateria
- 🔌 add-ons: **microSD** (capturas/logs), buzzer, LiPo, GPS (wardriving)
- 🔴 **Módulo ofensivo** (só pentest autorizado): rogue AP/evil twin, deauth, beacon flood,
  probe spoofing, downgrade, WPS, DoS de AP, beacon spoofing, GATT — ver §Ofensivo no doc de ideias

Cada uma vira um item do menu em `src/main.cpp`. Me diz qual você quer primeiro.

> ⚠️ Funções que transmitem (beacon spam, replay) interferem em terceiros — use só em
> ambiente/equipamentos seus ou autorizados.
