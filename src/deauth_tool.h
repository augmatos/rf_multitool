#pragma once
// ============================================================================
//  Desautenticacao (DoS em clientes)  -  SOMENTE PENTEST AUTORIZADO POR ESCRITO.
//
//  Diferente do Rogue AP, aqui NAO tem versao "so telemetria": o mecanismo em
//  si E a interrupcao de servico (derruba dispositivos da rede a forca). Por
//  isso os limites abaixo sao decisao de engenharia, nao so preferencia:
//
//   - Alvo SOMENTE de um scan ao vivo (mesmo padrao do Rogue AP) -- nunca
//     BSSID digitado livremente.
//   - Confirmacao explicita toda vez que o app abre (nao fica "lembrada").
//   - Rajada LIMITADA por acionamento (DEAUTH_BURST_FRAMES) -- nunca um modo
//     de flood continuo/indefinido.
//   - COOLDOWN entre rajadas -- mesmo segurando o botao, nao vira um jammer
//     continuo.
//
//  Envia deauth broadcast (afeta todos os clientes associados ao BSSID alvo).
//  Nao faz sniffing de MAC de cliente especifico nesta versao.
//
//  Uso responsavel: so em redes que voce tem AUTORIZACAO POR ESCRITO para
//  testar. Derrubar dispositivos de terceiros sem consentimento e crime na
//  maioria das jurisdicoes.
// ============================================================================
#include <Arduino.h>
#include <TFT_eSPI.h>

#define DEAUTH_BURST_FRAMES 40      // frames por rajada -- fixo, nao configuravel na UI
#define DEAUTH_COOLDOWN_MS  5000    // tempo minimo entre rajadas

class DeauthTool {
public:
  void begin();                // reseta pro estado de aviso/confirmacao
  void confirmStep();          // OK: avanca a maquina de estados / dispara rajada se armado
  void selMove(int d);         // girar: navega a lista de redes (fase SELECT)
  void cancel();                // BACK: sai (nao ha nada "ativo" continuamente pra parar)
  void draw(TFT_eSPI &tft, bool full);

private:
  enum State { ST_WARNING, ST_SCANNING, ST_SELECT, ST_CONFIRM, ST_ARMED, ST_NONE_FOUND };
  State  _state = ST_WARNING;
  int    _scanCount = 0;
  int    _sel = 0;
  String _targetSsid;
  uint8_t _targetBssid[6] = {0};
  int    _targetChannel = 1;

  uint32_t _lastBurstMs  = 0;
  uint32_t _burstsFired  = 0;

  void doScan();
  void fireBurst();
};
