# 02 — Testar cada módulo na bancada (ANTES de soldar)

A regra é: **valide tudo na protoboard, um módulo de cada vez.** Solda esconde defeitos;
protoboard deixa você refazer ligações em segundos. Só vá para a placa universal quando
**todos** os testes abaixo passarem.

## Material da bancada
- Protoboard + jumpers macho-macho/macho-fêmea.
- Multímetro (para conferir tensões e continuidade).
- Um controle remoto 433 MHz qualquer (portão, carrinho, tomada RF) para testar o CC1101.
- As antenas dos NRF **rosqueadas**.

## Como rodar um teste
Cada teste é um ambiente PlatformIO isolado. No terminal, dentro da pasta do projeto:

```bash
pio run -e test_display -t upload -t monitor   # troque o nome do -e conforme o passo
```
Ambientes: `test_display`, `test_encoder`, `test_cc1101`, `test_nrf24`, `test_wifi`.
Saia do monitor com **Ctrl+C**.

> **Serial não aparece?** No ESP32-S3, conecte pela porta **USB** (nativa). Se ainda
> assim nada: segure **BOOT**, conecte o USB, solte **BOOT** (entra em modo download),
> grave, e reabra o monitor. O `delay(300/400)` nos testes dá tempo do USB-CDC subir.

---

## Passo 0 — Sanidade (sem nada ligado)
1. Só a DevKit no USB. Rode `pio run -e test_wifi -t upload -t monitor`.
2. **Esperado:** a cada ~3 s aparece a lista de redes Wi-Fi ao redor.
3. Isso prova que o toolchain, a placa e o rádio interno funcionam. Se falhar aqui, o
   problema é ambiente/driver USB, não fiação.

## Passo 1 — Display ILI9341
Ligue **só o display** (ver tabela em [01-wiring.md](01-wiring.md)). VCC, GND, CS, DC,
RST, MOSI, SCK, LED (e MISO se quiser).
```bash
pio run -e test_display -t upload -t monitor
```
- **Esperado:** tela preta com "Display OK", 3 quadrados (🟥🟩🟦) e um contador subindo.
- **Tela branca/apagada:** confira **LED/backlight** (GPIO 21 em HIGH), VCC, GND, RST.
- **Nada / travado:** confira CS (10), DC (9), MOSI (11), SCK (12). Cabos curtos!
- **Cores trocadas (vermelho↔azul) ou imagem suja:** no `platformio.ini` troque
  `-DILI9341_2_DRIVER=1` por `-DILI9341_DRIVER=1` (ou vice-versa) e regrave.
- **Imagem espelhada/girada:** ajuste `UI_ROTATION` em `include/config.h` (0–3).

## Passo 2 — Encoder KY-040 + botões
Ligue o KY-040 (CLK 38, DT 39, SW 47, + em 3V3, GND) e os 3 botões (uma perna no GPIO
5/1/40, a outra no GND).
```bash
pio run -e test_encoder -t upload -t monitor
```
- **Esperado:** ao girar, `enc=` muda (sobe num sentido, desce no outro). Ao apertar,
  `SW`/`OK`/`BACK`/`AUX` vão para `1`.
- **Gira ao contrário:** inverta `ENC_DT`/`ENC_CLK` no `attachHalfQuad` (ou troque os
  fios CLK↔DT).
- **Pula de 2 em 2 (ou só conta 1 a cada 2 cliques):** ajuste `ENC_STEPS_PER_DETENT` no
  `config.h` (valores comuns: 1, 2 ou 4).
- **Botão sempre em 1:** está faltando o GND, ou ligou no pino errado.

## Passo 3 — CC1101 (sub-GHz)
Ligue só o CC1101 no barramento dos rádios (SCK 18, MOSI 17, MISO 8, CSN 7, GDO0 6,
VCC 3V3, GND). **Decoupling de 100 nF** ajuda.
```bash
pio run -e test_cc1101 -t upload -t monitor
```
- **Esperado:** `CC1101: OK (detectado)` e o RSSI imprimindo (ex.: `-95 dBm`).
  Acione um controle 433 MHz por perto: o RSSI **sobe** (ex.: para `-40 dBm`).
- **`FALHA`:** 99% das vezes é **MISO (8)**, **CSN (7)** ou **3V3/GND**. Confira a
  continuidade com o multímetro e que VCC está em 3.3 V (não 5 V).
- **RSSI não reage ao controle:** confira a antena/molinha do CC1101 e a frequência
  do seu controle (alguns são 315 MHz — ajuste `SUBGHZ_DEFAULT_MHZ`).

## Passo 4 — NRF24L01+PA+LNA (os dois)
**Antenas rosqueadas!** Ligue os dois NRF no barramento (SCK 18, MOSI 17, MISO 8;
#1: CE 4 / CSN 2; #2: CE 42 / CSN 41). **Alimente pelo regulador 3V3 dedicado** e
ponha **10 µF + 100 nF** no VCC de cada um.
```bash
pio run -e test_nrf24 -t upload -t monitor
```
- **Esperado:** `NRF #1: OK` e `NRF #2: OK`, depois uma linha de 80 caracteres por
  varredura: `.` = canal livre; dígito/`#` = atividade. Os canais do seu Wi-Fi devem
  acender (ligue o hotspot do celular do lado para confirmar).
- **`FALHA` (1 ou ambos):** quase sempre é **ALIMENTAÇÃO**. Sintomas: funciona no
  começo e some, ou só o #2 falha. Soluções, em ordem:
  1. Capacitor de **10 µF** (ou maior) direto no VCC/GND do NRF.
  2. Regulador 3V3 **dedicado** (não o 3V3 da DevKit).
  3. Fios curtos no VCC/GND.
- **`FALHA` constante mesmo com boa alimentação:** confira CSN/CE de cada um (não trocar
  #1 com #2) e o MISO compartilhado (8).

## Passo 5 — Tudo junto na protoboard (firmware principal)
Com display + encoder + botões + CC1101 + 2× NRF montados na protoboard:
```bash
pio run -e main -t upload -t monitor
```
1. Aparece o splash com o aviso de uso responsável → clique no encoder.
2. Vá em **Diagnóstico / Self-test**: deve mostrar `OK` para Display, CC1101, NRF #1, NRF #2.
3. Teste cada app (WiFi, 2.4GHz Analyzer, Sub-GHz Sweep/Monitor). `BACK` sai de cada um.

## Troubleshooting geral
| Sintoma | Causa provável | Solução |
|---|---|---|
| ESP reinicia ao usar NRF | Brownout (corrente) | Cap 10 µF + regulador 3V3 dedicado |
| Display funciona sozinho, mas falha com os rádios | Conflito de host SPI | Confirme `-DUSE_HSPI_PORT=1`; rádios no `SPI` global (pinos RAD_*) |
| CC1101 e NRF não funcionam juntos | CS errado / bus errado | Cada um com seu CS; SCK/MOSI/MISO iguais (18/17/8) |
| Leituras erráticas | GND solto | Junte TODOS os GND num ponto comum |
| Não grava firmware | Modo download | Segure BOOT, conecte USB, solte BOOT |

✅ **Só passe para a soldagem quando o Passo 5 estiver 100%.**
