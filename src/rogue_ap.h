#pragma once
// ============================================================================
//  Rogue AP / Evil Twin  -  SOMENTE PENTEST AUTORIZADO POR ESCRITO.
//
//  Clona o SSID (e canal) de uma rede JA VISTA num scan ao vivo -- nao aceita
//  nome digitado livremente, pra nao virar "clona qualquer rede que eu quiser
//  sem nem confirmar que ela existe por perto". Exige confirmacao explicita
//  TODA VEZ que o app abre (nao fica "lembrado" entre sessoes).
//
//  Escopo DELIBERADAMENTE limitado: mede taxa de associacao automatica
//  (quantos dispositivos entram sozinhos na rede clonada, sem senha) --
//  NAO tem portal cativo nem captura de credenciais aqui. Isso e o
//  suficiente pra um teste de "os dispositivos confiam soh no nome da rede?"
//  sem template pronto de phishing.
//
//  Uso responsavel: so em redes que voce tem AUTORIZACAO POR ESCRITO para
//  testar (engajamento de pentest formal). Nunca contra terceiros sem
//  consentimento -- isso e crime na maioria das jurisdicoes.
// ============================================================================
#include <Arduino.h>
#include <TFT_eSPI.h>

class RogueAp {
public:
  void begin();                    // reseta pro estado de aviso/confirmacao
  void confirmStep();              // OK: avanca a maquina de estados
  void selMove(int d);             // girar: navega a lista de redes (fase SELECT)
  void cancel();                   // BACK: cancela e para o AP se estiver ativo
  void draw(TFT_eSPI &tft, bool full);
  bool isRunning() const;

private:
  enum State { ST_WARNING, ST_SCANNING, ST_SELECT, ST_CONFIRM, ST_RUNNING, ST_NONE_FOUND };
  State  _state = ST_WARNING;
  int    _scanCount = 0;
  int    _sel = 0;
  String _targetSsid;
  int    _targetChannel = 1;

  void doScan();
  void startAp();
  void stopAp();
};
