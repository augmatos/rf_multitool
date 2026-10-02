# 04 — Funções e ideias para o dispositivo

Catálogo de funções legais para o multitool, separadas por categoria. Cada uma tem:
**dificuldade** (🟢 fácil / 🟡 média / 🔴 dá trabalho), o **hardware** que usa e, quando
faz sentido, um aviso ⚠️ de uso responsável.

> Lembre: beacon spam, deauth, replay e jamming **interferem em terceiros**. Use em
> **laboratório**, em redes/equipamentos **seus**, ou com **autorização**. As ideias de
> "rede engraçada" abaixo são pensadas como brincadeira controlada, não para poluir o
> espectro de uma vizinhança inteira.

---

## 🎭 Wi-Fi divertido / "picante"

### 1. Beacon spammer com SSIDs engraçados 🟡 ✅ IMPLEMENTADO (item de menu: **Beacon Spammer**)
Cria várias redes-fantasma com nomes de brincadeira que aparecem na lista de Wi-Fi de
quem está perto. Código em `src/beacon_spam.cpp` / `.h` (frames 802.11 via
`esp_wifi_80211_tx`, BSSID estável por rede, salto de canal 1/6/11, modo "história",
clique = liga/desliga, AUX = troca de lista). Lista editável no código. Ideias de nome:
- `Vai_Cair_Nessa_Rede?`, `FBI_Surveillance_Van_04`, `Wi-Fi_da_Vizinha_NÃO`
- `Carregando...`, `Conectando...`, `Procurando_redes...` (gozação visual)
- `Pinga_aqui_pra_internet`, `Senha_eh_12345`, `Boleto_Vence_Hoje`
- `Tela_Azul.exe`, `Definitivamente_Nao_Eh_Virus`, `Click_Aqui_Mãe`
- Modo "história": o nome muda sozinho e cada beacon conta um pedaço de uma frase 😄

⚠️ Beacon flooding pode atrapalhar a lista de redes de aparelhos próximos e é ilegal em
alguns lugares. Mantenha a lista **curta** e use em ambiente controlado.

### 2. Captive portal "pegadinha"/conscientização 🟡 (Wi-Fi + reaproveita o ESPhishing)
Em vez de roubar nada, o portal abre uma página tipo *"Você acabou de cair num teste de
phishing 👀 — nunca digite senha em Wi-Fi aberto!"* (ou um rickroll). Ótimo para
**treinamento de conscientização** e palestras. Conecta direto com o seu projeto ESPhishing.

### 3. SSID dinâmico / "letreiro" 🟢 (Wi-Fi interno)
O nome da própria rede muda a cada poucos segundos, virando um letreiro/relógio que
aparece na varredura dos outros. Inofensivo e engraçado.

---

## 📡 Radar e recon (passivo, "do bem")

### 4. "Radar de pessoas" — contador de dispositivos 🟡 ✅ IMPLEMENTADO (item de menu: **Radar de Pessoas**)
Conta quantos celulares/dispositivos diferentes estão por perto (probe requests + anúncios
BLE) e mostra um medidor de "lotação do ambiente". Divertido em festa, sala de aula,
escritório. Só conta, não identifica ninguém. Código em `src/radar.cpp` / `.h` (sniff passivo,
decay 2 min, gauge colorido + vibe de ambiente).

### 5. Detector de ataque deauth 🟡 (Wi-Fi monitor mode) ⭐
**Defensivo:** fica de olho e **avisa (LED/buzzer) se alguém está disparando deauth** por
perto (ataque comum de "derrubar Wi-Fi"). Útil de verdade e 100% do bem.

### 6. Detector de tracker/AirTag desconhecido 🟡 (BLE) ⭐
**Privacidade:** detecta um AirTag/Tile/SmartTag que está te "seguindo" (aparece em vários
lugares com você). Avisa na tela. Mesmo conceito do "alerta de rastreador" do celular.

### 7. Mapa de calor 2.4GHz + "melhor canal" 🟢 (NRF24 — você já tem o scanner)
Além do gráfico de ocupação, recomenda **o canal mais limpo** para você configurar seu
roteador. Prático no dia a dia.

### 8. Quem é o fabricante? (OUI lookup) 🟢 (Wi-Fi/BLE)
Mostra o fabricante dos dispositivos por perto pelo MAC (Apple, Samsung, Intel...). Curioso
para ver "tem muito iPhone nessa sala".

---

## 📻 Sub-GHz (CC1101)

### 9. Decodificar sensores 433 MHz da vizinhança 🟡 ⭐
Estações meteorológicas, sensores de temperatura/umidade sem fio e campainhas baratas
transmitem em 433 MHz **em texto quase aberto**. Dá para **mostrar a temperatura do sensor
do vizinho** na sua telinha. Curioso e impressiona.

### 10. Caça ao transmissor / "fox hunt" 🟡 ⭐
Medidor de RSSI com barra grande + bip que acelera quando você **chega perto** de um
transmissor escondido. Brincadeira ótima em grupo / CTF (esconde um controle e cada um
tenta achar).

### 11. Capturar e reproduzir o **seu** controle 🔴 ✅ IMPLEMENTADO (item de menu: **Sub-GHz Capturar/Repetir**)
Grava o sinal OOK/ASK de um controle (portão, lâmpada RF) e reproduz. Clássico dos
multitools. Código em `src/subghz_capture.cpp` / `.h`. Técnica: o CC1101 é colocado em
modo assíncrono "serial" mexendo direto nos registradores `MDMCFG2` (OOK/ASK, sem sync
word), `PKTCTRL0` (formato serial assíncrono, comprimento infinito) e `IOCFG0` (GDO0 =
dado serial assíncrono) — sem pacote/CRC, o GDO0 vira um logic-analyzer de 1 bit: uma
interrupção de borda grava a duração de cada pulso na captura (RX) e o mesmo pino é
"batido" em modo digital pra reproduzir o trem de pulsos gravado (TX). Tem **3 slots**
que persistem na NVS (Preferences, igual ao padrão do Tamagotchi), cada um guardando
também a **frequência em que foi gravado** — a reprodução sempre usa a frequência salva
no slot, nunca a selecionada na hora, pra nunca trocar de banda sozinha. Controles: OK
grava, AUX reproduz, girar o encoder troca de slot, clique do encoder troca a frequência
de captura entre os presets (433.92 / 315.00 / 390.00 / 868.35 MHz), BACK sai (salvando).
⚠️ **Só nos seus próprios equipamentos, e SÓ funciona com código FIXO** (estilo
PT2262/EV1527). Rolling code — a maioria dos carros e portões comerciais modernos, que
trocam o código a cada acionamento — **nunca** vai destrancar com uma gravação simples.
Nota: ruído ambiente na faixa pode "sujar" a captura, já que o GDO0 em modo assíncrono
fica sempre demodulando (não há squelch) — capture perto do controle e aperte o botão
dele durante a janela de gravação (~5 s).

### 12. Analisador de espectro com "waterfall" 🟡 ✅ IMPLEMENTADO — depois virou o **#21** (item de menu: **Waterfall Display**)
Varredura contínua desenhando uma cascata (waterfall) de RSSI na tela — fica bonito e
ajuda a enxergar onde tem atividade. Ver detalhes no item **#21** logo abaixo, pra onde essa
função foi promovida (upgrade com fonte dupla + scroll de verdade). Cor mapeada tipo
espectrograma: azul (fraco, -100 dBm) → ciano → verde → amarelo → vermelho (forte, -20 dBm).

---

## 🔵 Bluetooth LE

### 13. Scanner BLE 🟢 ✅ IMPLEMENTADO (item de menu: **Scanner BLE**)
Lista dispositivos BLE por perto (nome, RSSI, fabricante): fones, smartwatches, balanças,
etiquetas. Bom para "o que tem de BLE ligado aqui?". Implementado direto em `src/main.cpp`
(`appBleScan`, scan ativo bloqueante de 3s por rodada, igual ao WiFi Scanner). Fabricante só
é mostrado pro ID `0x004C` (Apple — bem documentado por causa do iBeacon/AirTag); os demais
IDs da lista da Bluetooth SIG não foram conferidos, de propósito, pra não chutar errado —
dá pra completar a tabela depois se quiser mais fabricantes reconhecidos.

### 14. iBeacon/Eddystone próprio 🟢
Seu aparelho vira um beacon BLE (ex.: anuncia seu nome/portfólio). Inofensivo e legal de
mostrar.

---

## 🧰 Utilidades

### 15. QR Code de Wi-Fi na tela 🟢 ⭐
Gera um **QR** que, ao ser escaneado, **conecta o convidado na sua rede** sem digitar
senha. Super útil em casa. (Dá para gerar QR do seu LinkedIn/`github.com/augmatos` também 😉.)

### 16. Relógio + clima 🟢 (Wi-Fi)
Puxa hora (NTP) e previsão do tempo e mostra na tela quando o aparelho está "parado".

### 17. Monitor de bateria 🟢 (precisa do divisor de tensão — ver [01-wiring.md §6](01-wiring.md))
Mostra a % da LiPo num cantinho da tela.

---

## 🎮 Joguinhos / pet (encoder + tela)

### 18. Snake / Pong / Dino com o encoder 🟢
Joguinhos clássicos controlados pelo KY-040. Mata o tempo e testa display+input.

### 19. "Tamagotchi de RF" 🟡 ⭐ ✅ IMPLEMENTADO (item de menu: **RF Tamagotchi**)
Um bichinho na tela que fica **mais feliz e ganha XP quanto mais sinais de rádio ele
"come"** (redes Wi-Fi + canais 2.4G ativos = comida). Gamifica o scanning — ambiente cheio
= evolui rápido; vazio = fome. Tem 5 formas (Ovo→Blobi→Pixel→Fera→Anciao), barras de XP/
barriga/humor, carinho pelo encoder e **estado salvo na flash (NVS)** entre reinícios.
Código em `src/tamagotchi.cpp` / `.h`. Ideia futura: somar BLE como mais uma fonte de comida.

---

## 📊 Diagnóstico e telemetria (fáceis, "do bem")

### 20. Wi-Fi Signal Logger 🟢 (Wi-Fi)
Trava numa rede/SSID e mostra o **RSSI em tempo real** num gráfico rolante, com
**média, mínimo e máximo**. Ótimo pra achar o **melhor posicionamento de AP** (andar pela
casa vendo onde o sinal cai). Combina com buzzer (bip mais rápido = sinal melhor).

### 21. Waterfall Display 🟡 ✅ IMPLEMENTADO (item de menu: **Waterfall Display**)
Cascata de RSSI ao longo do tempo × frequência/canal — visual **tipo mini-SDR**. Era a
evolução gráfica do #12, e agora É o app (o #12 antigo foi promovido pra isso — não tem
dois itens de waterfall no menu). Implementado em `src/main.cpp` (`appWaterfall`), com
**duas fontes** trocáveis pelo botão **AUX** (só se os dois módulos estiverem presentes):
- **CC1101 sub-GHz**: RSSI real, varrendo `SUBGHZ_SWEEP_START/END` com passo fino
  (`WATERFALL_STEP` = 0.02 MHz no `config.h`).
- **NRF24 2.4GHz**: os 80 canais, RPD convertido em "pseudo-RSSI" (quantas de 8 escutas
  curtas viram portadora no canal, mapeado pra -100..-20 dBm pra usar a mesma escala de cor).

Diferente da primeira versão, agora o histórico faz **scroll de verdade**: um buffer em RAM
(`malloc`, ~43KB, com checagem de falha de alocação) guarda `ROWS×PIXW` pixels; a cada
"tick" o buffer inteiro é deslocado uma linha (`memmove`) e a linha nova entra no topo, tudo
reenviado pra tela numa única chamada `tft.pushImage` (mais rápido e mais confiável do que
ler a tela de volta via SPI, que depende do driver/fiação do MISO do display). Trocar de
fonte limpa o histórico (`memset`) pra não misturar colunas com significados diferentes.
Cor mapeada tipo espectrograma: azul (fraco) → ciano → verde → amarelo → vermelho (forte),
com barra de legenda embaixo.

### 22. Uptime & Temperatura 🟢 ✅ IMPLEMENTADO (item de menu: **Uptime & Sistema**)
Painel de status do próprio aparelho: **temperatura do ESP32** (`temperatureRead()`),
**memória livre** (`ESP.getFreeHeap()`) e **uptime**. Zero hardware extra. Implementado em
`src/main.cpp` (`appUptime`). Além do básico pedido, também mostra: **heap livre mínimo já
visto** (bom pra flagrar vazamento de memória em sessões longas), **maior bloco alocável**
(`ESP.getMaxAllocHeap()`, mais útil que só "heap livre" pra saber se dá pra alocar algo
grande, tipo o buffer do Waterfall), modelo/revisão do chip e frequência da CPU
(`ESP.getChipModel/getChipRevision/getCpuFreqMHz`), e tamanho da flash. ⚠️ O sensor de
temperatura interno do ESP32-S3 **não é calibrado** — serve como referência relativa
(ex.: "esquentou desde que ligou o Wi-Fi"), não como termômetro preciso; isso está avisado
na tela.

### 23. Achievements / conquistas 🟡 ✅ IMPLEMENTADO (item de menu: **Achievements**)
Sistema de troféus salvos na flash. Código em `src/achievements.cpp`/`.h` (classe
`Achievements`, Preferences/NVS). 11 conquistas na v1:
- **WiFi:** primeira rede, 10/100/1000 redes (soma cumulativa de "avistamentos" entre
  scans, não são MACs únicos — dedup de verdade é o que o Radar já faz).
- **Uso das ferramentas:** primeiro Sub-GHz Sweep, primeira gravação no Capturar/Repetir,
  primeiro Scanner BLE.
- **Uptime:** 1h / 10h / 100h de uso total **acumulado entre boots** (soma o tempo de
  sessão a cada 30s na NVS, não zera ao reiniciar).
- **Tamagotchi:** bichinho evoluiu até a forma "Fera" (nível 6+).

Casa com o **Tamagotchi**: cada conquista nova dá **+20 XP** de bônus, guardado
(`drainXpBonus()`) e aplicado na próxima vez que o app do bichinho for aberto
(`pet.addXp(...)`, novo método no `tamagotchi.cpp`). Tela de conquistas tem lista rolável
pelo encoder (`[X]`/`[ ]`), descrição + progresso da selecionada (ex.: "42/100 redes") e um
toast "Desbloqueado: ..." quando uma nova é destravada. Fácil de continuar adicionando metas
(basta crescer `ACH_COUNT` e as tabelas `NAMES`/`DESCS`/`enum` no `.cpp`).

### (Battery Monitor)
Já está listado como **#17** (precisa do divisor de tensão — ver [01-wiring.md §6](01-wiring.md)).

---

## 🔴 Ofensivo — pentest Wi-Fi/BLE (SÓ com autorização por escrito)

> ⛔ **Leia antes.** As funções abaixo **atacam/derrubam/enganam** redes e clientes.
> Só fazem sentido num **engajamento de pentest autorizado por escrito**, CTF ou
> **laboratório fechado seu**. Usar contra terceiros é crime na maioria dos países.
> Quando formos implementar, cada uma virá com trava de "lab mode" e aviso na tela.
>
> Nota técnica: em cores ESP32 recentes, a injeção de alguns frames (ex.: **deauth**)
> pelo `esp_wifi_80211_tx()` pode vir **bloqueada** — precisa de core/patch específico.
> Isso a gente avalia na hora de cada uma.

### 24. Rogue AP / Evil Twin 🔴 ⚠️ ✅ IMPLEMENTADO (item de menu: **Rogue AP (PENTEST!)**)
Sobe um AP com o **mesmo SSID e canal** de uma rede-alvo pra medir associação automática
de clientes. Código em `src/rogue_ap.cpp`/`.h` (classe `RogueAp`, máquina de estados
Aviso → Scan → Seleção → Confirmação → Rodando).

**Escopo deliberadamente limitado** (decisão tomada com o Augusto antes de implementar,
contexto: pentest autorizado por escrito):
- **Confirmação explícita toda vez** que o app abre — não fica "lembrada" entre sessões.
  Duas telas de confirmação: uma genérica de responsabilidade, outra mostrando o **nome
  exato** da rede que vai ser clonada antes de ligar o AP.
- **Só clona SSID de rede JÁ VISTA num scan ao vivo** — não aceita nome digitado
  livremente. Isso impede "clonar qualquer coisa que eu quiser sem nem confirmar que ela
  existe por perto", reduzindo o uso contra alvos arbitrários/não verificados.
- **Sem portal cativo, sem captura de credenciais.** A tela mostra só
  `WiFi.softAPgetStationNum()` (quantos dispositivos entraram sozinhos, sem senha, na
  rede aberta clonada) — o suficiente pra um relatório de pentest de "os dispositivos
  confiam só no nome da rede?", sem virar um kit de phishing pronto. Integrar com o
  ESPhishing pra capturar credenciais é uma decisão separada e maior, não incluída aqui.
- Banner de aviso fixo em toda tela do app; BACK sempre para o AP e sai imediatamente
  (`WiFi.softAPdisconnect(true)` + `WiFi.mode(WIFI_OFF)`).

⚠️ Nunca contra terceiros sem consentimento — isso é crime na maioria das jurisdições.

### 25. Desautenticação (DoS em clientes) 🔴 ⚠️ ✅ IMPLEMENTADO (item de menu: **Deauth (PENTEST!)**)
Envia frames **deauth** pra derrubar a associação de clientes de um AP. Código em
`src/deauth_tool.cpp`/`.h` (classe `DeauthTool`, mesma máquina de estados do Rogue AP:
Aviso → Scan → Seleção → Confirmação → **Armado**).

Diferença importante em relação ao Rogue AP: aqui **não existe versão "só telemetria"** —
o mecanismo em si é a interrupção de serviço, não tem como medir sem derrubar de verdade.
Por isso, quando perguntei ao Augusto como limitar o risco, ele escolheu só "confirmação
toda vez que abre" entre as opções — mas os outros três limites (rajada curta, alvo só de
scan, cooldown) foram mantidos **como decisão de engenharia**, não como preferência
opcional, porque são o que diferencia "ferramenta de teste pontual" de "jammer contínuo de
bolso". Uma caixa de confirmação sozinha não limita capacidade técnica, só é clicada uma
vez e esquecida.

Limites implementados:
- **Alvo só de scan ao vivo** — igual ao Rogue AP, nunca BSSID digitado livre.
- **Confirmação explícita toda vez que o app abre** (como pedido) — duas telas, incluindo
  uma bem direta ("Isto DERRUBA dispositivos reais... não é uma simulação").
- **Rajada fixa e limitada**: `DEAUTH_BURST_FRAMES = 40` frames por acionamento — não
  configurável na UI de propósito.
- **Cooldown de `DEAUTH_COOLDOWN_MS = 5000` (5s)** entre rajadas — mesmo segurando o botão,
  não vira flood contínuo.
- Envia **deauth broadcast** (destino `FF:FF:FF:FF:FF:FF`, origem spoofada = BSSID do AP
  alvo) — afeta todos os clientes associados àquele AP. **Não** faz sniffing de MAC de
  cliente específico nesta versão (mais simples, e broadcast já é o ataque clássico
  padrão tipo `aireplay-ng -0` sem `-c`).
- Contador de rajadas disparadas na tela, com aviso pra registrar no relatório do teste.

⚠️ Nota técnica: em cores ESP32 mais recentes, `esp_wifi_80211_tx()` pode filtrar/bloquear
frames de deauth dependendo da versão do SDK — não testado em hardware real por mim; se o
ataque não surtir efeito, pode ser isso (não é um bug do código, é uma trava do próprio SDK
Espressif). Nunca contra terceiros sem consentimento — DoS é crime na maioria das
jurisdições.

### 26. Beacon Flood 🟡 ⚠️ — ❌ NÃO feito como "flood"; ✅ virou modo **Stress-test** limitado
O "flood" irrestrito (poluição em massa não-direcionada de TODA a lista de redes de quem
está no alcance) **não foi implementado de propósito** — é um DoS que atinge terceiros sem
consentimento, e mesmo em pentest você não tem autorização de todo aparelho no raio.
Decisão tomada com o Augusto (contexto: pentest autorizado), e alinhada com a preferência
registrada de **manter os limites técnicos das features ofensivas mesmo quando não pedidos**.

Em vez disso, virou um **modo "Stress-test"** dentro do **Beacon Spammer** (`src/beacon_spam.cpp`,
`AUX` cicla Engracado → Historia → Stress-test), pensado pra estressar **UM aparelho alvo
autorizado**, com todos os freios:
- **Confirmação de autorização** ao entrar no modo (clique do encoder) — não persiste.
- **Dead-man switch:** só transmite enquanto o botão **OK está SEGURADO**; soltou, para na hora.
- **Teto de SSIDs por rajada** (`STRESS_MAX_SSIDS` = 50, ajustável pelo encoder de 1 a 50).
- **Cooldown** entre rajadas (`STRESS_COOLDOWN_MS` = 300 ms → no máx ~3 rajadas/s).
- **SSIDs neutros** (`RF-TEST-00`…), claramente de teste, não nomes enganosos.
- Aviso na tela de que **mesmo assim irradia pra quem estiver perto** (beacon é broadcast),
  então: ficar perto do alvo e usar por pouco tempo.

Alternativa defensiva ainda em aberto (não feita): **detector de beacon flood** (avisar
quando alguém está inundando beacons por perto) — 100% do bem, dá pra fazer quando quiser.

### 27. Probe Request / Response Spoofing 🔴 ⚠️
Forja probes pra sondar/atrair clientes e mapear redes que os dispositivos "procuram".

### 28. Downgrade de segurança 🔴 ⚠️
Induz cliente/AP a cair pra um esquema de segurança mais fraco (teste de configuração).

### 29. Ataques contra WPS 🔴 ⚠️
Testa PIN/PBC do WPS (fraqueza clássica de roteadores domésticos).

### 30. DoS contra AP 🔴 ⚠️
Sobrecarga/associações forjadas pra medir a resistência do AP a negação de serviço.

### 31. Beacon Spoofing 🔴 ⚠️ ✅ IMPLEMENTADO (item de menu: **Beacon Spoofing**)
Clona **SSID + BSSID reais** de um AP existente (diferente do spam de nomes aleatórios).
Código em `src/beacon_spoof.cpp` / `.h` (scan ao vivo, seleciona alvo, transmite no canal
exato, cooldown entre rajadas, LED de atividade).

### 32. Enumeração agressiva de serviços GATT (BLE) 🟡 ⚠️
Varre a fundo os serviços/características GATT de um dispositivo BLE-alvo (recon profundo).

---

## 🔌 Hardware extra que destrava funções
| Add-on | Destrava |
|---|---|
| **Cartão microSD** (no bus do display) | gravar capturas/logs/pcap, salvar perfis de freq., screenshots, listas de SSID |
| **Buzzer / motor de vibração** | feedback do fox-hunt, alarme do detector de deauth/tracker (lembre do diodo flyback!) |
| **Bateria LiPo + TP4056** | uso portátil de verdade (ver power-path em [01-wiring.md §4](01-wiring.md)) |
| **GPS (UART)** | wardriving: mapear redes com coordenadas e salvar no SD |

---

## 🏆 Por onde eu começaria (opinião)
1. **#1 Beacon spammer com nomes engraçados** — é o que você pediu e dá um retorno
   imediato e divertido.
2. **#10 Fox hunt** e **#19 Tamagotchi de RF** — usam o que você já montou (RSSI, tela,
   encoder) e são as mais "gostosas" de mostrar.
3. **#5 Detector de deauth** e **#6 Detector de tracker** — as mais úteis/defensivas.
4. **#9 Decodificar sensores 433** e **#15 QR de Wi-Fi** — alto "fator uau" com pouco código.

Cada uma vira um item no menu (`src/main.cpp`). Me diz qual você quer primeiro que eu
implemento.
