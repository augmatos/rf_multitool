# 03 — Soldar na placa universal 10×10 cm

> 🔑 **Princípio nº 1: nada é soldado direto.** Solde apenas **barras de pino fêmea**
> (soquetes), passivos (resistores/capacitores) e fios. O ESP32-S3, o display, o CC1101
> e os dois NRF **encaixam** nos soquetes. Assim você troca qualquer módulo que falhe sem
> dessoldar nada — e módulos RF clones falham com frequência.

## Material
- Barras de pino fêmea (femea) suficientes para: ESP32-S3 DevKit (2 fileiras), display
  (9 pinos), CC1101 (8 pinos), 2× NRF24 (8 pinos cada).
- Fio rígido fino AWG 28–30 (ou fios jumper cortados). Use **cores**: 🔴 vermelho = 3V3,
  ⚫ preto = GND, outras cores para sinais.
- Solda 0,8 mm, ferro 30–60 W (idealmente com controle de temperatura ~330 °C), fluxo,
  esponja/lã de bronze.
- Capacitores **cerâmicos** 100 nF ("104") ×4–5 — um por módulo, decoupling.
- Capacitores **eletrolíticos** 10 µF ×3 (um por rádio) + bulk 100–470 µF ×1 (na entrada/3V3-RF).
- Resistores 220–330 Ω ×2 (LEDs). (Opcional: 2× 100 kΩ p/ monitor de bateria.)
- **Diodo Schottky** 1N5817/SS34 ×1 (proteção de polaridade na entrada; ×2 se for fazer
  power-path USB+LiPo).
- Regulador AMS1117-3.3 (módulo) ou MP1584.
- Opcional: chave liga/desliga, conector da bateria, módulo TP4056 (carga da LiPo).
- Multímetro (continuidade + tensão) — **indispensável**.

> **Onde vai cada passivo (detalhe em [01-wiring.md §4](01-wiring.md)):** 100 nF cerâmico
> colado no VCC/GND de cada módulo; 10 µF eletrolítico junto ao VCC de cada NRF/CC1101;
> bulk 100–470 µF na entrada e na trilha 3V3-RF; 220–330 Ω em série com cada LED; Schottky
> em série na entrada de energia.
> ⚠️ **Polaridade:** eletrolítico (faixa "−" → GND) e diodo (faixa = cátodo, lado da saída).
> Cerâmico não tem polaridade.

## Planta de posicionamento (vista de cima, placa 10×10)

```
 +----------------------------------------------------------+
 |  CC1101            [ trilha 3V3-RF + caps ]      NRF #1   |  <- topo: rádios e
 |  (soquete)                                       (antena↑)|     antenas para fora
 |                                                           |
 |                  +-------------------------+      NRF #2  |
 |   AMS1117-3.3     |     ESP32-S3 DevKit      |     (antena↑)|
 |   (regulador)     |   (2 fileiras de soquete)|             |
 |   bulk 470uF      +-------------------------+              |
 |                                                           |
 |   [==== trilha 3V3 ====]   [==== trilha GND ====]         |  <- barramentos de
 |                                                           |     alimentação
 |  LED LED   (o)encoder    [BACK][OK][AUX]                  |  <- frente: controles
 +----------------------------------------------------------+
            ( display ILI9341 monta na frente, sobre soquetes,
              ou ligado por fora com um cabo flat curto )
```

Princípios do layout:
- **Rádios no topo, antenas apontando para fora** e afastadas entre si e do ESP32.
- **ESP32 no centro**, deitado, ocupando a largura (a DevKit é larga).
- **Controles na frente** (encoder, botões, LEDs) — onde a mão alcança.
- **Alimentação numa faixa própria** (regulador + caps + trilhas 3V3/GND).
- Mantenha os fios **SCK/MOSI/MISO curtos** e longe das antenas.

## Barramentos de alimentação
Crie duas "trilhas" ao longo de uma fileira de furos da placa, usando fio rígido
desencapado passando por vários furos (ou as próprias trilhas de cobre da placa, se for
do tipo com barramentos). Tenha:
- Trilha **GND** (comum a TUDO).
- Trilha **3V3** (do LDO da DevKit) para display, encoder, botões.
- Trilha **3V3-RF** (do AMS1117 dedicado) para CC1101 + 2× NRF, com o **bulk 470 µF** na
  entrada e **10 µF + 100 nF** perto de cada rádio.

## Ordem de soldagem (faça nesta sequência)
1. **Marque** a posição de cada módulo a seco (encaixe tudo sem soldar e confira que cabe
   e que as antenas ficam para fora).
2. **Soquetes do ESP32-S3** primeiro (são a referência). Solde 1 pino de cada barra,
   confira o alinhamento, depois solde o resto.
3. **Trilhas GND e 3V3** (alimentação). Solde e teste continuidade ponta a ponta.
4. **Diodo Schottky de entrada** (faixa apontando para o circuito) + **chave liga/desliga**,
   se for usar. É a proteção contra inversão de polaridade — vem antes do regulador.
5. **Regulador 3V3-RF + bulk 100–470 µF** → solde a trilha 3V3-RF. **Ligue só isso** e meça:
   a saída deve dar **3.3 V** antes de qualquer rádio entrar. (Eletrolítico: faixa "−" → GND.)
6. **Soquetes dos módulos** (CC1101, NRF #1, NRF #2, display). Em cada um, solde o par de
   desacoplamento **bem rente ao VCC/GND**: **100 nF cerâmico + 10 µF eletrolítico** em
   paralelo (cerâmico = sem polaridade; eletrolítico = faixa "−" no GND).
7. **Fios de sinal:** comece pelo barramento compartilhado dos rádios (SCK 18, MOSI 17,
   MISO 8) ligando aos 3 soquetes; depois os CS/CE/GDO individuais; depois o display.
8. **Encoder, botões e LEDs.** Cada LED com um resistor **220–330 Ω** em série (perna longa
   do LED = ânodo, vai para o lado do GPIO via resistor; perna curta = GND). Botões: uma
   perna no GPIO, a outra no GND (sem resistor — pull-up interno).
9. **Bateria/TP4056** (se for usar): Schottky/load-sharing conforme [01-wiring.md §4](01-wiring.md).

## Inspeção ANTES de energizar (multímetro)
- **Curto 3V3↔GND?** Meça continuidade entre 3V3 e GND, e entre 3V3-RF e GND: **não pode
  apitar**. Se apitar, há um curto — ache antes de ligar.
- **Continuidade de cada sinal** do pino do ESP até o soquete do módulo (bipe = ok).
- **Solda fria / pontes:** olhe contra a luz; reaqueça pontos foscos; remova pontes de
  solda entre pinos vizinhos com malha dessoldadora.

## Primeira energização
1. **Sem nenhum módulo encaixado**, ligue o USB. Meça 3V3 e 3V3-RF (devem dar ~3.3 V).
   Se possível, veja a corrente: poucas dezenas de mA é normal só com a DevKit.
2. Desligue. **Encaixe um módulo por vez** (display → CC1101 → NRF #1 → NRF #2),
   religando e conferindo que nada esquenta anormalmente entre cada um.
3. **Antenas rosqueadas** nos NRF antes de energizar com eles encaixados.

## Verificação final (mesmos testes da bancada, agora na placa)
Regrave os ambientes de teste **na placa soldada** para validar a solda:
```bash
pio run -e test_display -t upload -t monitor
pio run -e test_encoder -t upload -t monitor
pio run -e test_cc1101  -t upload -t monitor
pio run -e test_nrf24   -t upload -t monitor
```
Depois o firmware final e o **Diagnóstico** no menu:
```bash
pio run -e main -t upload -t monitor
```
Se um teste que passava na protoboard falha agora, o problema é **solda** (ponto frio,
ponte ou fio trocado) naquele módulo específico — vá direto nele.

## Dicas finais
- 🧷 **Alívio de tensão** no cabo USB (um furo + abraçadeira) evita arrancar trilha.
- 🏷️ **Etiquete** os fios de sinal nas pontas enquanto solda — você vai agradecer.
- 🔩 **Standoffs/espaçadores** nos 4 cantos da placa protegem a solda por baixo.
- 🧪 Teste de continuidade **sempre** antes de aplicar energia — 5 minutos que salvam
  módulos de R$ caros.
- ♻️ Como tudo está em soquete, se um NRF clone vier com defeito, é só trocar a peça.
