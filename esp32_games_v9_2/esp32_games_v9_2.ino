/*
 * PIXEL ARCADE v9.1 - 24 jogos | niveis com meta de pontos | 4 dificuldades
 *
 * NOVIDADES DA V9.2: acesso com SENHA. Na pagina de atualizacao (secao 5) escolha:
 *   aberto / senha so para o admin / senha para usuario e admin. Sair: /logout.
 * NOVIDADES DA V9.1: imagem de fundo sem cores estranhas (com pre-visualizacao e reinicio automatico),
 *   cor de fundo do menu corrigida, bateria/carregando (BETA, desligada por padrao), correcao do erro do SURF.
 * NOVIDADES DA V9: 2 jogos novos (SURF e INVASORES), botao DE NOVO ao fim do jogo,
 *   tela de RECORDES, brilho da tela, sair do jogo com 2 toques no X, site mostra nivel/pontos.
 * NOVIDADES DA V8:
 *   - O ESP32 NAO cria mais WiFi proprio e NAO tem login/admin.
 *     Ele entra no WiFi / hotspot do SEU CELULAR (so aceita 2.4 GHz).
 *   - Site de controle funciona no celular E no computador
 *     (computador: setas/WASD, Espaco/Enter = A, X = B, Esc = menu, e o mouse na aba Toque).
 *   - Controle (usuario):  http://IP/esp32
 *   - Atualizar + personalizar (nomes dos jogos, fundo do menu, enviar .bin):
 *     http://IP/update/adimesp32   (tem as instrucoes na propria pagina)
 *   - Tambem atualiza pelo cabo USB (Arduino IDE) e pelo WiFi na Arduino IDE (porta "pixelarcade")
 *   - Todos os jogos tem NIVEIS com META DE PONTOS (10 niveis) e, ao iniciar,
 *     voce escolhe a dificuldade: FACIL, NORMAL, MEDIO ou DIFICIL.
 *   - Jogo novo: CONTAS (matematica). Agora sao 22 jogos.
 *   - Hora de Rondonia: automatica pela internet do celular + ajuste manual (CFG > HORA).
 *
 * COMO CONECTAR:
 *   1. Ligue o hotspot (ponto de acesso) do celular. IMPORTANTE: banda 2.4 GHz!
 *      (iPhone: ligue "Maximizar compatibilidade".)
 *   2. No ESP32: CFG > WIFI > escolha a rede > digite a senha. Fica salvo.
 *      (Opcional: escreva o nome/senha em WIFI_SSID_DEF / WIFI_PASS_DEF logo abaixo.)
 *   3. No menu toque em "QR": aparece o endereco (ex: 192.168.43.50/esp32) e um QR code.
 *      Abra esse endereco no navegador do celular ou do computador (mesmo WiFi).
 *
 * IMPORTANTE: na Arduino IDE use Tools > Partition Scheme = "Default 4MB with spiffs".
 *
 * PLACA: ESP32-2432S028 ("CYD" - 2.8" ILI9341 + touch XPT2046)
 *
 * BIBLIOTECAS (Gerenciador de Bibliotecas da Arduino IDE):
 *   - TFT_eSPI            (Bodmer)
 *   - XPT2046_Touchscreen (Paul Stoffregen)
 *   (WiFi, WebServer e o gerador de QR Code ja estao dentro do proprio codigo)
 *
 * CONFIGURACAO DA TFT_eSPI (arquivo User_Setup.h da biblioteca):
 *
 *   #define ILI9341_2_DRIVER
 *   #define TFT_WIDTH  240
 *   #define TFT_HEIGHT 320
 *   #define TFT_MISO 12
 *   #define TFT_MOSI 13
 *   #define TFT_SCLK 14
 *   #define TFT_CS   15
 *   #define TFT_DC    2
 *   #define TFT_RST  -1
 *   #define TFT_BL   21
 *   #define TFT_BACKLIGHT_ON HIGH
 *   #define USE_HSPI_PORT
 *   #define LOAD_GLCD
 *   #define LOAD_FONT2
 *   #define LOAD_FONT4
 *   #define SPI_FREQUENCY       55000000
 *   #define SPI_READ_FREQUENCY  20000000
 *   // cores invertidas? adicione: #define TFT_INVERSION_ON
 *
 * Placa na IDE: "ESP32 Dev Module"
 */

#include <TFT_eSPI.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <LittleFS.h>
#include <ArduinoOTA.h>

// ---------------- Rede (WiFi do celular) ----------------
#define WIFI_SSID_DEF ""            // opcional: nome do hotspot do celular
#define WIFI_PASS_DEF ""            // opcional: senha do hotspot
#define TZ_OFFSET_H (-4)            // Rondonia = UTC-4 (sem horario de verao)

// ---------------- Pinos ----------------
#define SND_PIN 26
#define BL_PIN  21
#define T_CLK   25
#define T_MISO  39
#define T_MOSI  32
#define T_CS    33
#define T_IRQ   36

#define C565(r,g,b) ((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3))
#define NGAMES 24
#define PAGE_SZ 6
#define NPAGES ((NGAMES + PAGE_SZ - 1) / PAGE_SZ)

// botoes do controle do celular / computador
#define K_UP    1
#define K_DOWN  2
#define K_LEFT  4
#define K_RIGHT 8
#define K_A     16
#define K_B     32
#define K_MENU  64

TFT_eSPI tft = TFT_eSPI();
SPIClass tsSPI(VSPI);
XPT2046_Touchscreen ts(T_CS, T_IRQ);
Preferences prefs;
WebServer server(80);

// ---------------- Tema / configuracoes / rede ----------------
#define VERSION "9.2"
uint16_t BG = TFT_BLACK, FG = TFT_WHITE, SUBC = TFT_LIGHTGREY, ACC = TFT_YELLOW, ACC2 = TFT_CYAN;
int themeLight = 0, musicOn = 1, invertOn = 0, gammaMode = 1;
String wifiSsid = "", wifiPass = "";
bool needRedraw = false;
bool staUp = false, mdnsOk = false, wifiBusy = false, otaOn = false;
uint32_t ntpLast = 0, netLast = 0, wifiTry = 0;
const char *defNames[NGAMES] = {"FLAPPY", "COBRA", "BLOCOS", "TOCA", "GENIUS", "CORRIDA",
                                "PONG", "VELHA", "NAVE", "TORRE", "2048", "TETRIS", "LABIRINTO",
                                "FRUTAS", "CARROS", "MEMORIA", "REFLEXO", "SALTO", "MINAS", "ALVO", "LIGA4",
                                "CONTAS", "SURF", "INVASORES"};

String gname[NGAMES];                 // nomes dos jogos (editaveis em /update/adimesp32)
uint16_t menuBg = 0;
bool menuBgSet = false, bgOn = false;
// ---- acesso ao site (senha) ----
uint8_t accMode = 0;               // 0 = aberto | 1 = senha no usuario e no admin | 2 = senha so no admin
String accPass = "", accTok = "";  // senha guardada e token da sessao (muda ao reiniciar)
bool fwOk = false;                 // a atualizacao por arquivo foi autorizada?

void netPoll();
void sndUpdate();
void menuDraw();
int lvScoreNow();
void gameInit(int g);

void setTheme() {
  if (themeLight) { BG = C565(238, 240, 250); FG = TFT_BLACK; SUBC = C565(70, 70, 90); ACC = C565(200, 90, 0); ACC2 = C565(0, 80, 190); }
  else { BG = TFT_BLACK; FG = TFT_WHITE; SUBC = TFT_LIGHTGREY; ACC = TFT_YELLOW; ACC2 = TFT_CYAN; }
}

const uint8_t gammaCur[3] = {0x01, 0x04, 0x02};   // normal, vivo, suave
void applyDisplay() {
  tft.invertDisplay(invertOn);
  tft.writecommand(0x26);
  tft.writedata(gammaCur[gammaMode]);
}

// ---------- Bateria / carregando (BETA - o ESP so funciona na tomada, entao vem DESLIGADA) ----------
// Para usar: ligue a bateria (LiPo) por um divisor de tensao 2x100k no pino BAT_PIN e ligue em CFG (toque no canto direito
// da linha de baixo). Se o carregador tiver pino CHRG, coloque o numero dele em CHG_PIN (LOW = carregando).
#define BAT_PIN 35
#define BAT_DIV 2.0f
#define CHG_PIN -1
bool batBeta = false, batHas = false, batChg = false;
int batPct = -1, batRise = 0;
float batV = 0, batPrev = 0;
uint32_t batT = 0;

int batCurve(float v) {
  static const float V[8] = {3.30f, 3.60f, 3.73f, 3.80f, 3.87f, 3.98f, 4.10f, 4.20f};
  static const int P[8] = {0, 10, 20, 40, 60, 80, 90, 100};
  if (v <= V[0]) return 0;
  if (v >= V[7]) return 100;
  for (int i = 1; i < 8; i++)
    if (v <= V[i]) return P[i - 1] + (int)((v - V[i - 1]) * (P[i] - P[i - 1]) / (V[i] - V[i - 1]));
  return 100;
}

void batInit() {
  analogSetPinAttenuation(BAT_PIN, ADC_11db);
#if CHG_PIN >= 0
  pinMode(CHG_PIN, INPUT_PULLUP);
#endif
}

bool batUpdate(bool force) {            // true = mudou algo para redesenhar
  if (!batBeta) { if (batHas) { batHas = false; return true; } return false; }
  uint32_t now = millis();
  if (!force && batT && now - batT < 5000) return false;
  batT = now;
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) sum += analogReadMilliVolts(BAT_PIN);
  float v = sum / 16.0f / 1000.0f * BAT_DIV;
  bool has = (v > 3.0f && v < 4.6f);
  int pct = -1;
  bool chg = false;
  if (has) {
    pct = batCurve(v);
#if CHG_PIN >= 0
    chg = (digitalRead(CHG_PIN) == LOW);
#else
    if (v > batPrev + 0.002f) batRise = min(batRise + 1, 4);
    else if (v < batPrev - 0.002f) batRise = max(batRise - 1, 0);
    chg = (batRise >= 2) || v >= 4.17f;
#endif
  }
  batPrev = v; batV = v;
  bool changed = (has != batHas) || pct != batPct || chg != batChg;
  batHas = has; batPct = pct; batChg = chg;
  return changed;
}

String batText() {
  if (!batBeta) return String("BATERIA: OFF");
  if (!batHas) return String("SEM BATERIA");
  return String(batPct) + "%" + (batChg ? " CARREGANDO" : "");
}

int bright = 5;                       // brilho da tela 1..5
void setBright(int b) {
  int duty = constrain(b, 1, 5) * 51;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(BL_PIN, duty);
#else
  ledcWrite(1, duty);
#endif
}
void brightInit() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(BL_PIN, 5000, 8);
#else
  ledcSetup(1, 5000, 8);
  ledcAttachPin(BL_PIN, 1);
#endif
  setBright(bright);
}

void showMsg(const char *a, String b) {
  tft.fillScreen(BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(ACC, BG);
  tft.drawString(a, 160, 100, 4);
  tft.setTextColor(FG, BG);
  tft.drawString(b, 160, 140, 2);
}

// ---------------- Estado global ----------------
enum { MENU = 0, G_FLAPPY, G_COBRA, G_BLOCOS, G_TOCA, G_GENIUS, G_CORRIDA,
       G_PONG, G_VELHA, G_NAVE, G_TORRE, G_2048, G_TETRIS, G_LABIR, G_FRUTAS, G_CARROS,
       G_MEMORIA, G_REFLEXO, G_SALTO, G_MINAS, G_ALVO, G_LIGA4, G_CONTAS, G_SURF, G_INVAS };
int mode = MENU;
int mpage = 0;
int hs[NGAMES] = {0};

// ---------------- Dificuldade e niveis ----------------
#define LVMAX 10                                   // passar do nivel 10 = zerou o jogo
int diff = 1;                                      // 0 facil, 1 normal, 2 medio, 3 dificil
int lvl = 1;                                       // nivel atual
int lvGoal = 5;                                    // meta de pontos do nivel atual
bool wonAll = false;
const char *DNAME[4] = {"FACIL", "NORMAL", "MEDIO", "DIFICIL"};
const float DM[4] = {0.75f, 1.0f, 1.25f, 1.5f};

// velocidade/dificuldade geral = dificuldade escolhida x nivel
float SPD() { return DM[diff] * (1.0f + 0.10f * (lvl - 1)); }

// vidas: facil +2, normal igual, medio -1, dificil -2 (minimo 1)
int liv(int b) { static const int o[4] = {2, 0, -1, -2}; return max(1, b + o[diff]); }

// pontos necessarios por nivel (no NORMAL). Cresce a cada nivel.
const uint16_t GSTEP[NGAMES] = {5, 6, 100, 8, 2, 100, 3, 2, 12, 8, 500, 400, 2, 8, 8, 6, 80, 25, 40, 12, 2, 6, 40, 24};
int lvStep(int g, int level) {
  static const float gm[4] = {0.7f, 1.0f, 1.25f, 1.5f};
  float s = GSTEP[g] * gm[diff] * (1.0f + 0.15f * (level - 1));
  return max(1, (int)(s + 0.5f));
}

// ---------------- Relogio (hora de Rondonia) ----------------
bool clockSet = false;
uint64_t baseEpoch = 0;
uint32_t baseMs = 0;

bool getClock(int &h, int &m) {
  if (!clockSet) return false;
  time_t t = (time_t)(baseEpoch + (millis() - baseMs) / 1000) + TZ_OFFSET_H * 3600;
  struct tm tmv;
  gmtime_r(&t, &tmv);
  h = tmv.tm_hour; m = tmv.tm_min;
  return true;
}

// ajuste manual da hora (so mostra HH:MM, a data nao importa)
void setClockManual(int h, int m) {
  long long e = 1767225600LL + (long long)h * 3600 + (long long)m * 60 - (long long)TZ_OFFSET_H * 3600;
  baseEpoch = (uint64_t)e;
  baseMs = millis();
  clockSet = true;
  ntpLast = millis();
}

void wifiConnect() {
  if (!wifiSsid.length()) return;
  WiFi.disconnect(false);
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  wifiTry = millis();
}

void netTask() {            // WiFi do celular -> hora pela internet + reconexao automatica
  uint32_t now = millis();
  if (now - netLast < 1000) return;
  netLast = now;
  if (wifiBusy) return;
  if (WiFi.status() == WL_CONNECTED) {
    if (!staUp) {
      staUp = true;
      configTime(0, 0, "pool.ntp.org", "time.google.com", "a.st1.ntp.br");
      if (!mdnsOk && MDNS.begin("pixelarcade")) { mdnsOk = true; MDNS.addService("http", "tcp", 80); }
      ntpLast = 0;
      ArduinoOTA.setHostname("pixelarcade");
      ArduinoOTA.onStart([]() { showMsg("ATUALIZANDO...", "nao desligue o ESP"); });
      ArduinoOTA.begin();
      otaOn = true;
    }
    time_t t = time(nullptr);
    if (t > 1600000000 && (ntpLast == 0 || now - ntpLast > 600000UL)) {
      baseEpoch = (uint64_t)t; baseMs = millis(); clockSet = true; ntpLast = now;
      if (ntpLast == 0) ntpLast = 1;
    }
  } else {
    if (staUp) { staUp = false; if (otaOn) { ArduinoOTA.end(); otaOn = false; } if (mdnsOk) { MDNS.end(); mdnsOk = false; } }
    if (wifiSsid.length() && now - wifiTry > 20000UL) wifiConnect();
  }
}

void netPoll() {            // atende o site, o som e a internet
  server.handleClient();
  if (otaOn) ArduinoOTA.handle();
  sndUpdate();
  netTask();
}

// ---------------- Touch + controle virtual ----------------
bool touched = false, pressed = false, prevT = false;
int tx = 0, ty = 0;

// entradas vindas do celular / computador
uint8_t padHeld = 0, padE = 0, padEL = 0;   // botoes segurados / novos toques
bool webDown = false, webTapL = false;
int wx = 160, wy = 120;
int vpx = 160, vpy = 120;                   // posicao virtual (setas arrastando)
uint32_t lastInput = 0;

// converte as setas do controle em "toques" para os jogos antigos
void padToTouch() {
  switch (mode) {
    case G_FLAPPY: case G_CORRIDA: case G_TORRE: case G_REFLEXO:
      if (padE & (K_A | K_B | K_UP)) { tx = 160; ty = 120; touched = true; }
      break;
    case G_COBRA: case G_CARROS:
      if (padE & K_LEFT)       { tx = 10;  ty = 120; touched = true; }
      else if (padE & K_RIGHT) { tx = 310; ty = 120; touched = true; }
      break;
    case G_BLOCOS: case G_NAVE: case G_FRUTAS: case G_SALTO: case G_INVAS:
      if (padHeld & (K_LEFT | K_RIGHT)) {
        vpx = constrain(vpx + ((padHeld & K_RIGHT) ? 8 : -8), 0, 319);
        tx = vpx; ty = 120; touched = true;
      }
      break;
    case G_PONG:
      if (padHeld & (K_UP | K_DOWN)) {
        vpy = constrain(vpy + ((padHeld & K_DOWN) ? 7 : -7), 20, 239);
        tx = 20; ty = vpy; touched = true;
      }
      break;
    case G_GENIUS:
      if (padE & K_UP)         { tx = 80;  ty = 75;  touched = true; }
      else if (padE & K_RIGHT) { tx = 240; ty = 75;  touched = true; }
      else if (padE & K_LEFT)  { tx = 80;  ty = 185; touched = true; }
      else if (padE & K_DOWN)  { tx = 240; ty = 185; touched = true; }
      break;
    case G_SURF:
      if (padE & K_LEFT)                { tx = 10;  ty = 120; touched = true; }
      else if (padE & K_RIGHT)          { tx = 310; ty = 120; touched = true; }
      else if (padE & (K_UP | K_A))     { tx = 160; ty = 60;  touched = true; }
      else if (padE & (K_DOWN | K_B))   { tx = 160; ty = 200; touched = true; }
      break;
    case G_CONTAS:
      if (padE & K_UP)         { tx = 80;  ty = 135; touched = true; }
      else if (padE & K_RIGHT) { tx = 240; ty = 135; touched = true; }
      else if (padE & K_LEFT)  { tx = 80;  ty = 198; touched = true; }
      else if (padE & K_DOWN)  { tx = 240; ty = 198; touched = true; }
      break;
    default: break;   // 2048, TETRIS, LABIRINTO leem padE/padHeld direto
  }
}

void readTouch() {
  touched = false;
  if (ts.tirqTouched() && ts.touched()) {
    TS_Point p = ts.getPoint();
    tx = constrain(map(p.x, 200, 3700, 0, 320), 0, 319);
    ty = constrain(map(p.y, 240, 3800, 0, 240), 0, 239);
    touched = true;
  }

  // controle do celular / computador
  padE = padEL; padEL = 0;
  if ((webDown || padHeld) && millis() - lastInput > 700) { webDown = false; padHeld = 0; }  // seguranca
  if (!touched) {
    if (webTapL)      { webTapL = false; tx = wx; ty = wy; touched = true; }
    else if (webDown) { tx = wx; ty = wy; touched = true; }
    else              padToTouch();
  }

  pressed = touched && !prevT;
  prevT = touched;
}

bool anyConfirm() { return pressed || (padE & (K_A | K_B)); }

void waitTap() {
  do { netPoll(); readTouch(); delay(10); } while (touched);
  do { netPoll(); readTouch(); delay(10); } while (!anyConfirm());
}

// ---------------- Servidor web (controle: celular e computador) ----------------
const char PAGE[] PROGMEM = R"rawliteral(<!doctype html><html lang="pt-BR"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">
<title>Controle Pixel Arcade V9.1</title><style>
:root{--bg:#0f0d1e;--fg:#eeecff;--card:#1a1733;--bd:#2d2952;--ac:#5a3df0;--ac2:#9a86ff;--mu:#9d9ac0;--ye:#ffc233;--area:#000}
@media(prefers-color-scheme:light){:root{--bg:#f1f0fb;--fg:#1a1733;--card:#fff;--bd:#cfcbe8;--mu:#5d5a80;--ye:#c76a00;--area:#dcdcec}}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent;user-select:none;-webkit-user-select:none}
html,body{margin:0}
body{background:var(--bg);color:var(--fg);font:16px system-ui,sans-serif;padding:env(safe-area-inset-top) 12px env(safe-area-inset-bottom);min-height:100vh}
#w{max-width:1000px;margin:0 auto}
#top{display:flex;justify-content:space-between;align-items:center;padding:12px 2px;font-weight:700}
#top span{color:var(--ye)}
#dot{display:inline-block;width:10px;height:10px;border-radius:50%;background:#e44;margin-right:8px}
#dot.ok{background:#3c6}
#tabs{display:flex;gap:8px;margin-bottom:12px}
#tabs button{flex:1;padding:10px;border-radius:10px;border:2px solid var(--bd);background:var(--card);color:inherit;font:inherit;font-weight:700;cursor:pointer}
#tabs .on{background:var(--ac);border-color:var(--ac);color:#fff}
.sec.off{display:none}
#pad{display:grid;grid-template-columns:1fr 1fr;gap:14px;align-items:center}
#dp{display:grid;grid-template-columns:repeat(3,1fr);grid-template-rows:repeat(3,1fr);gap:6px;aspect-ratio:1;touch-action:none}
.k{border:2px solid var(--bd);background:var(--card);color:inherit;border-radius:14px;font:inherit;font-size:22px;display:flex;align-items:center;justify-content:center;touch-action:none;cursor:pointer}
.k.on{background:var(--ac);border-color:var(--ac2);color:#fff}
#ab{display:flex;gap:16px;align-items:center;justify-content:center}
.r{width:92px;height:92px;border-radius:50%;font-size:28px;font-weight:800}
.r.b{margin-top:56px}
#mn{margin-top:14px;width:100%;padding:14px;font-size:16px;font-weight:700}
#area{width:100%;aspect-ratio:4/3;background:var(--area);border:2px solid var(--ac);border-radius:12px;display:flex;align-items:center;justify-content:center;color:var(--mu);text-align:center;padding:10px;touch-action:none;cursor:crosshair}
.m{color:var(--mu);font-size:13px;text-align:center;margin-top:10px}
#main{display:block}
@media(min-width:820px){
 #main{display:grid;grid-template-columns:1.3fr 1fr;gap:22px;align-items:start}
 #tabs{display:none}
 .sec.off{display:block}
 #c2{order:1}#c1{order:2}
}
</style></head><body><div id="w">
<div id="top"><b><i id="dot"></i>PIXEL ARCADE V9.1</b><span id="g">Menu</span></div>
<div id="tabs"><button id="t1" class="on">Controle</button><button id="t2">Toque</button></div>
<div id="main">
<div id="c1" class="sec"><div id="pad">
<div id="dp"><span></span><button class="k" data-b="1">▲</button><span></span>
<button class="k" data-b="4">◀</button><span></span><button class="k" data-b="8">▶</button>
<span></span><button class="k" data-b="2">▼</button><span></span></div>
<div id="ab"><button class="k r" data-b="16">A</button><button class="k r b" data-b="32">B</button></div></div>
<button class="k" id="mn" data-b="64">☰ Voltar ao menu</button></div>
<div id="c2" class="sec off"><div id="area">Toque ou clique e arraste aqui.<br>É o mesmo que tocar na tela do ESP32.</div></div>
</div>
<p class="m" id="st" style="font-weight:700"></p>
<p class="m" id="hint">Use o toque para escolher um jogo no menu.</p>
<p class="m">Computador: setas ou WASD • Espaço/Enter = A • X = B • Esc = menu • mouse na aba Toque</p>
</div>
<script>
const $=i=>document.getElementById(i);
const N=['Menu','Flappy','Cobra','Blocos','Toca','Genius','Corrida','Pong','Velha','Nave','Torre','2048','Tetris','Labirinto','Frutas','Carros','Memoria','Reflexo','Salto','Minas','Alvo','Liga4','Contas','Surf','Invasores'];
const TOUCH=[0,4,8,16,17,19,20,21];
const H={0:'Toque no jogo que quer jogar (ou use ◀ ▶ ▲ ▼ e A).',1:'A pula.',2:'◀ ▶ viram a cobra.',3:'◀ ▶ movem a raquete.',4:'Use a aba Toque.',5:'Setas = cores do Genius.',6:'A pula.',7:'▲ ▼ movem a raquete.',8:'Use a aba Toque.',9:'◀ ▶ movem a nave.',10:'A solta o bloco.',11:'Setas deslizam os números.',12:'◀ ▶ mover, ▲ girar, ▼ descer, B queda.',13:'Setas movem a bolinha.',14:'◀ ▶ movem a cesta.',15:'◀ ▶ trocam de faixa.',16:'Toque nas cartas.',17:'Toque (ou A) quando ficar verde.',18:'◀ ▶ movem o pulador.',19:'Toque abre, segure = bandeira.',20:'Toque nos alvos.',21:'Toque na coluna.',22:'▲ ▶ ◀ ▼ = os 4 botões de resposta (cima-esq, cima-dir, baixo-esq, baixo-dir).',23:'◀ ▶ trocam de faixa, ▲/A pula, ▼/B rola.',24:'◀ ▶ movem a nave (ela atira sozinha).'};
let M=0,E=0,X=160,Y=120,T=0,Q=0,busy=0,dirty=0,cm=-1;
function tab(n){$('c1').classList.toggle('off',n!=1);$('c2').classList.toggle('off',n!=2);$('t1').className=n==1?'on':'';$('t2').className=n==2?'on':''}
function mode(m){if(m==cm)return;cm=m;$('g').textContent=N[m]||'';$('hint').textContent=H[m]||'';tab(TOUCH.includes(m)?2:1)}
const D=['Facil','Normal','Medio','Dificil'];
function info(a){$('st').textContent=(+a[0])?('Nivel '+a[1]+' • '+a[2]+'/'+a[3]+' pontos • '+D[+a[4]]):''}
function send(){if(busy){dirty=1;return}busy=1;
 const u='/i?b='+M+'&e='+E+'&x='+(X|0)+'&y='+(Y|0)+'&t='+T+'&q='+Q;E=0;Q=0;
 fetch(u,{cache:'no-store'}).then(r=>r.text()).then(m=>{busy=0;$('dot').className='ok';const a=m.split(',');mode(+a[0]);info(a);if(dirty){dirty=0;send()}})
 .catch(()=>{busy=0;$('dot').className='';$('g').textContent='sem conexão'})}
function mark(m,on){document.querySelectorAll('[data-b="'+m+'"]').forEach(b=>b.classList.toggle('on',!!on))}
document.querySelectorAll('[data-b]').forEach(b=>{const m=+b.dataset.b;
 const dn=e=>{e.preventDefault();M|=m;E|=m;b.classList.add('on');if(navigator.vibrate)navigator.vibrate(8);send()};
 const up=e=>{e.preventDefault();M&=~m;b.classList.remove('on');send()};
 b.addEventListener('pointerdown',dn);['pointerup','pointercancel','pointerleave'].forEach(t=>b.addEventListener(t,up))});
const KM={ArrowUp:1,w:1,W:1,ArrowDown:2,s:2,S:2,ArrowLeft:4,a:4,A:4,ArrowRight:8,d:8,D:8,' ':16,Enter:16,z:16,Z:16,x:32,X:32,Shift:32,Escape:64,m:64,M:64};
addEventListener('keydown',e=>{const m=KM[e.key];if(!m)return;e.preventDefault();if(e.repeat)return;M|=m;E|=m;mark(m,1);send()});
addEventListener('keyup',e=>{const m=KM[e.key];if(!m)return;e.preventDefault();M&=~m;mark(m,0);send()});
addEventListener('blur',()=>{M=0;T=0;document.querySelectorAll('.k.on').forEach(b=>b.classList.remove('on'));send()});
const A=$('area'),pt=e=>{const r=A.getBoundingClientRect();
 X=Math.max(0,Math.min(319,(e.clientX-r.left)*320/r.width));Y=Math.max(0,Math.min(239,(e.clientY-r.top)*240/r.height))};
A.onpointerdown=e=>{A.setPointerCapture(e.pointerId);pt(e);T=1;Q=1;send()};
A.onpointermove=e=>{if(T){pt(e);send()}};
A.onpointerup=A.onpointercancel=()=>{T=0;send()};
$('t1').onclick=()=>tab(1);$('t2').onclick=()=>tab(2);
setInterval(()=>{if(T||M)send()},150);
setInterval(send,1200);
function hora(){fetch('/t?e='+Date.now()).catch(()=>{})}
hora();setInterval(hora,30000);send();
</script></body></html>)rawliteral";

// ---------------- Acesso com senha ----------------
bool needAuth(bool admin) {
  if (accMode == 0 || accPass.length() == 0) return false;
  if (accMode == 2) return admin;   // modo 2: so o admin pede senha
  return true;                      // modo 1: usuario e admin pedem senha
}

bool isAuthed() {
  if (accTok.length() == 0) return false;
  return server.header("Cookie").indexOf("pa=" + accTok) >= 0;
}

void sendLogin(String to, bool erro) {
  String s = "<!doctype html><html lang=pt-BR><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'><title>Senha - Pixel Arcade</title><style>"
    "body{font:16px system-ui;margin:40px auto;max-width:360px;padding:0 16px;background:#0f0d1e;color:#eee}input,button{font:inherit;padding:10px;width:100%;box-sizing:border-box;border-radius:8px;border:1px solid #444;background:#1a1733;color:#eee;margin:6px 0}"
    "button{background:#5a3df0;border:0;font-weight:700}.e{color:#ff7a7a}</style></head><body><h2>PIXEL ARCADE</h2>"
    "<form method=post action=/login><input type=hidden name=to value=\"" + to + "\">"
    "<input type=password name=p placeholder='Senha de acesso' autofocus required><button>Entrar</button></form>";
  if (erro) s += "<p class=e>Senha errada.</p>";
  s += "</body></html>";
  server.send(200, "text/html", s);
}

// Chame no inicio de cada rota protegida. Se devolver true, a rota ja respondeu e deve parar.
bool blocked(bool admin) {
  if (!needAuth(admin) || isAuthed()) return false;
  String u = server.uri();
  if (u == "/esp32" || u == "/update/adimesp32") sendLogin(u, false);
  else server.send(401, "text/plain", "senha necessaria: abra /esp32 e entre com a senha");
  return true;
}

void handleLogin() {
  String to = server.hasArg("to") ? server.arg("to") : String("/esp32");
  if (to != "/update/adimesp32") to = "/esp32";
  if (server.method() == HTTP_GET) { sendLogin(to, false); return; }
  if (accPass.length() > 0 && accMode != 0 && server.arg("p") == accPass) {
    accTok = String(esp_random(), HEX) + String(esp_random(), HEX);
    server.sendHeader("Set-Cookie", "pa=" + accTok + "; Path=/; Max-Age=2592000; HttpOnly");
    server.sendHeader("Location", to);
    server.send(302, "text/plain", "");
    return;
  }
  sendLogin(to, true);
}

void handleLogout() {
  accTok = "";
  server.sendHeader("Set-Cookie", "pa=; Path=/; Max-Age=0");
  server.sendHeader("Location", "/login");
  server.send(302, "text/plain", "");
}

void handleAccSave() {
  if (blocked(true)) return;
  int m = constrain(server.arg("m").toInt(), 0, 2);
  String p = server.arg("p");
  p.trim();
  if (p.length() > 16) p = p.substring(0, 16);
  if (p.length() && p != accPass) { accPass = p; accTok = ""; }   // senha nova: quem estava logado precisa entrar de novo
  if (accPass.length() == 0) m = 0;                                // sem senha nao da para pedir senha
  accMode = m;
  prefs.putUChar("acc", accMode);
  prefs.putString("apw", accPass);
  server.sendHeader("Location", "/update/adimesp32");
  server.send(302, "text/plain", "");
}

void handleInput() {
  if (blocked(false)) return;
  padHeld = server.arg("b").toInt();
  padEL |= (uint8_t)server.arg("e").toInt();
  wx = constrain(server.arg("x").toInt(), 0, 319);
  wy = constrain(server.arg("y").toInt(), 0, 239);
  webDown = server.arg("t").toInt() != 0;
  if (server.arg("q").toInt()) webTapL = true;
  lastInput = millis();
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/plain", String(mode) + "," + String(lvl) + "," + String(mode == MENU ? 0 : lvScoreNow()) + "," + String(lvGoal) + "," + String(diff));
}

void handleTime() {            // o navegador manda a hora (UTC em milissegundos)
  if (blocked(false)) return;
  uint64_t ms = strtoull(server.arg("e").c_str(), NULL, 10);
  if (ms > 1600000000000ULL) {
    baseEpoch = ms / 1000;
    baseMs = millis();
    clockSet = true;
    ntpLast = millis();
  }
  server.send(200, "text/plain", "ok");
}

uint8_t hv(char c) { return (c >= 'a') ? c - 'a' + 10 : (c >= 'A' ? c - 'A' + 10 : c - '0'); }

String hesc(String t) {
  t.replace("&", "&amp;"); t.replace("\"", "&quot;"); t.replace("<", "&lt;"); t.replace(">", "&gt;");
  return t;
}

String cleanName(String t, int mx) {
  String o = "";
  for (unsigned i = 0; i < t.length() && (int)o.length() < mx; i++) {
    char c = t[i];
    if (c >= 'a' && c <= 'z') c -= 32;
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_' || c == '.') o += c;
  }
  o.trim();
  return o;
}

const char ADMIN_JS[] PROGMEM = R"rawliteral(<script>
const $=i=>document.getElementById(i),pg=$('pg');
let img=null,px=null;
function half(src,tw,th){let cur=src,cw=src.naturalWidth||src.width,ch=src.naturalHeight||src.height;
 while(cw/2>=tw&&ch/2>=th){const t=document.createElement('canvas');t.width=Math.floor(cw/2);t.height=Math.floor(ch/2);
 const g=t.getContext('2d');g.imageSmoothingEnabled=true;g.imageSmoothingQuality='high';g.drawImage(cur,0,0,t.width,t.height);cur=t;cw=t.width;ch=t.height}
 return cur}
function render(){
 const W=320,H=240,c=document.createElement('canvas');c.width=W;c.height=H;const x=c.getContext('2d',{willReadFrequently:true});
 const iw=img.naturalWidth,ih=img.naturalHeight,cover=$('fit').value=='cover';
 const sc=cover?Math.max(W/iw,H/ih):Math.min(W/iw,H/ih),w=Math.round(iw*sc),h=Math.round(ih*sc),ox=Math.round((W-w)/2),oy=Math.round((H-h)/2);
 let fill='#000',bm=$('bgm').value;
 if(bm=='white')fill='#fff';
 else if(bm=='auto'){const t=document.createElement('canvas');t.width=t.height=1;const g=t.getContext('2d');g.drawImage(img,0,0,1,1,0,0,1,1);const p=g.getImageData(0,0,1,1).data;if(p[3]>128)fill='rgb('+p[0]+','+p[1]+','+p[2]+')'}
 x.fillStyle=fill;x.fillRect(0,0,W,H);
 x.imageSmoothingEnabled=true;x.imageSmoothingQuality='high';x.drawImage(half(img,w,h),ox,oy,w,h);
 const d=x.getImageData(0,0,W,H).data,dim=1-$('dim').value/100,buf=new Float32Array(W*H*3);
 for(let i=0;i<W*H;i++){buf[i*3]=d[i*4]*dim;buf[i*3+1]=d[i*4+1]*dim;buf[i*3+2]=d[i*4+2]*dim}
 const out=new Uint8Array(W*H*2),pv=$('pv').getContext('2d'),pi=pv.createImageData(W,H);
 const q=(v,m)=>Math.round(Math.max(0,Math.min(255,v))*m/255);
 const add=(xx,yy,f,er,eg,eb)=>{if(xx<0||xx>=W||yy>=H)return;const j=(yy*W+xx)*3;buf[j]+=er*f;buf[j+1]+=eg*f;buf[j+2]+=eb*f};
 for(let y=0;y<H;y++)for(let xx=0;xx<W;xx++){const i=(y*W+xx)*3;
  const r5=q(buf[i],31),g6=q(buf[i+1],63),b5=q(buf[i+2],31),rr=r5*255/31,gg=g6*255/63,bb=b5*255/31;
  const er=buf[i]-rr,eg=buf[i+1]-gg,eb=buf[i+2]-bb;
  add(xx+1,y,7/16,er,eg,eb);add(xx-1,y+1,3/16,er,eg,eb);add(xx,y+1,5/16,er,eg,eb);add(xx+1,y+1,1/16,er,eg,eb);
  const v=(r5<<11)|(g6<<5)|b5,k=y*W+xx;out[k*2]=v&255;out[k*2+1]=v>>8;
  pi.data[k*4]=rr;pi.data[k*4+1]=gg;pi.data[k*4+2]=bb;pi.data[k*4+3]=255}
 pv.putImageData(pi,0,0);px=out;pg.textContent='Pronto para enviar.'}
$('im').onchange=async()=>{const f=$('im').files[0];if(!f)return;img=new Image();img.src=URL.createObjectURL(f);await img.decode();render()};
['fit','bgm','dim'].forEach(k=>$(k).oninput=()=>{if(img)render()});
async function up(){
 if(!px){pg.textContent='Escolha uma imagem primeiro.';return}
 const hx=a=>{let s='';for(let i=0;i<a.length;i++)s+=(a[i]<16?'0':'')+a[i].toString(16);return s};
 let a=1,b=0;for(let i=0;i<px.length;i++){a=(a+px[i])%65521;b=(b+a)%65521}
 const ck=((b<<16)|a)>>>0;
 for(let o=0;o<px.length;o+=1024){let ok=0;
  for(let t=0;t<4&&!ok;t++){try{const r=await fetch('/bg?o='+o,{method:'POST',body:hx(px.subarray(o,o+1024))});ok=r.ok}catch(e){}}
  if(!ok){pg.textContent='Erro ao enviar. Tente de novo.';return}
  pg.textContent='Enviando '+Math.round(o*100/px.length)+'%'}
 const rb=$('rbt').checked,r2=await fetch('/bgdone?c='+ck+'&r='+(rb?1:0));
 pg.textContent=await r2.text()+(rb?' O ESP vai reiniciar...':'')}
function rb(){if(confirm('Reiniciar o ESP agora?'))fetch('/reboot').then(()=>pg.textContent='Reiniciando...')}
function bo(){fetch('/bgoff').then(()=>alert('Imagem removida'))}
function bon(){fetch('/bgon').then(r=>r.text()).then(t=>alert(t))}
</script></body></html>)rawliteral";

void handleAdmin() {
  if (blocked(true)) return;
  String s;
  s.reserve(9000);
  s += "<!doctype html><html lang=pt-BR><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'><title>Atualizar - Pixel Arcade</title><style>"
       "body{font:16px system-ui;margin:12px auto;max-width:640px;padding:0 10px;background:#0f0d1e;color:#eee}input,button{font:inherit;padding:8px;margin:3px 0;width:100%;box-sizing:border-box;border-radius:8px;border:1px solid #444;background:#1a1733;color:#eee}"
       "button{background:#5a3df0;border:0;font-weight:700}h3{color:#ffc233;margin:18px 0 4px}.g{display:grid;grid-template-columns:1fr 1fr;gap:6px}.box{background:#1a1733;border-radius:10px;padding:10px 14px;font-size:15px}a{color:#9a86ff}"
       "@media(prefers-color-scheme:light){body{background:#f4f3fb;color:#111}input,.box{background:#fff;color:#111}h3{color:#b35c00}}</style></head><body>";
  s += "<h2>PIXEL ARCADE v" VERSION " - Atualizar e personalizar</h2>";
  s += "<div class=box><b>Como atualizar o programa (3 formas):</b><ol>"
       "<li><b>Cabo USB (Arduino IDE):</b> ligue o ESP32 no computador, escolha a placa \"ESP32 Dev Module\" e a porta COM, e clique em Carregar (Upload). Funciona sempre.</li>"
       "<li><b>Por esta pagina (sem cabo):</b> na Arduino IDE use <i>Sketch &gt; Exportar binario compilado</i>. Na pasta do sketch vai aparecer o arquivo terminado em <b>.ino.bin</b> (NAO use o que tem 'bootloader' ou 'partitions'). Envie esse arquivo no campo abaixo. O ESP reinicia sozinho.</li>"
       "<li><b>Pelo WiFi na Arduino IDE:</b> com o computador e o ESP na mesma rede, em <i>Ferramentas &gt; Porta</i> aparece a porta de rede <b>pixelarcade</b>. Escolha ela e clique em Carregar.</li></ol>"
       "Dicas: use o esquema de particao <b>Default 4MB with spiffs</b>. Nao desligue o ESP durante o envio. Seus recordes, WiFi e configuracoes continuam salvos depois de atualizar.</div>";
  String ip = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : String("nao conectado");
  s += "<p>Endereco do ESP: " + ip + " &nbsp;|&nbsp; <a href=/esp32>Abrir o controle</a></p>";
  s += "<h3>1) Atualizar o programa (arquivo .ino.bin)</h3><form method=post action=/fw enctype=multipart/form-data><input type=file name=f accept=.bin required><button>Enviar e instalar</button></form>";
  s += "<form method=post action=/save>";
  s += "<h3>2) Nomes dos jogos (ate 10 letras)</h3><div class=g>";
  for (int i = 0; i < NGAMES; i++) s += "<input name=g" + String(i) + " maxlength=10 value=\"" + hesc(gname[i]) + "\">";
  s += "</div><h3>3) Cor de fundo do menu</h3>";
  char hx[8];
  snprintf(hx, sizeof(hx), "#%02x%02x%02x", ((menuBg >> 11) & 31) * 255 / 31, ((menuBg >> 5) & 63) * 255 / 63, (menuBg & 31) * 255 / 31);
  s += "<input type=color name=bgc value=" + String(hx) + " oninput=\"document.getElementById('tema').checked=false\">";
  s += "<label><input type=checkbox id=tema name=bgreset value=1 style=width:auto" + String(menuBgSet ? "" : " checked") + "> usar a cor do tema (ao mudar a cor acima, isto desmarca sozinho)</label><div style=font-size:13px;opacity:.8>Ao salvar uma cor, a imagem de fundo e desligada (ela continua guardada; use Ativar imagem guardada).</div>";
  s += "<button>Salvar nomes e cor</button></form>";
  s += "<h3>4) Imagem de fundo do menu</h3><input type=file id=im accept=image/*>"
       "<label>Como encaixar:<select id=fit style='width:100%;padding:8px;border-radius:8px'><option value=contain>Mostrar a imagem inteira (melhor para logos)</option><option value=cover>Preencher a tela (corta as bordas)</option></select></label>"
       "<label>Cor das bordas:<select id=bgm style='width:100%;padding:8px;border-radius:8px'><option value=auto>Automatica (cor do canto da imagem)</option><option value=black>Preto</option><option value=white>Branco</option></select></label>"
       "<label>Escurecer (deixa as letras do menu mais legiveis):<input type=range id=dim min=0 max=70 value=0></label>"
       "<div>Pre-visualizacao (como vai ficar na tela do ESP):</div><canvas id=pv width=320 height=240 style='width:100%;max-width:320px;border:1px solid #666;border-radius:8px'></canvas>"
       "<label><input type=checkbox id=rbt checked style=width:auto> Reiniciar o ESP sozinho depois de enviar (recomendado)</label>"
       "<button onclick=up()>Enviar imagem</button><button onclick=bon()>Ativar imagem guardada</button><button onclick=bo()>Remover imagem</button><div id=pg></div>";
  s += "<h3>5) Acesso ao site (senha)</h3><form method=post action=/acc><div class=box>"
       "<label>Quem precisa de senha:<select name=m style='width:100%;padding:8px;border-radius:8px'>"
       "<option value=0" + String(accMode == 0 ? " selected" : "") + ">Aberto (qualquer um na mesma rede)</option>"
       "<option value=1" + String(accMode == 1 ? " selected" : "") + ">Usuario e admin (controle e esta pagina pedem senha)</option>"
       "<option value=2" + String(accMode == 2 ? " selected" : "") + ">So o admin (controle livre, esta pagina pede senha)</option></select></label>"
       "<label>Senha de acesso (ate 16 caracteres):<input name=p maxlength=16 value=\"" + hesc(accPass) + "\" autocomplete=off></label>"
       "</div><button>Salvar acesso</button></form>"
       "<div style=font-size:13px;opacity:.8>Se trocar a senha, quem estiver logado precisa entrar de novo. <a href=/logout>Sair desta sessao</a>. Esqueceu a senha? Na Arduino IDE use Ferramentas &gt; Erase All Flash Before Sketch Upload e carregue o programa de novo (isso apaga WiFi, recordes e configuracoes).</div>";
  s += "<h3>6) Reiniciar</h3><div style=font-size:13px;opacity:.8>Reinicia o ESP como se fosse uma atualizacao (ajuda a imagem de fundo a carregar certinho).</div><button onclick=rb()>Reiniciar o ESP agora</button>";
  s += ADMIN_JS;
  server.send(200, "text/html", s);
}

void handleSave() {
  if (blocked(true)) return;
  for (int i = 0; i < NGAMES; i++) {
    String g = cleanName(server.arg("g" + String(i)), 10);
    if (g.length()) {
      gname[i] = g;
      char key[6]; snprintf(key, sizeof(key), "n%d", i);
      prefs.putString(key, g);
    }
  }
  String c = server.arg("bgc");
  if (server.arg("bgreset") == "1") menuBgSet = false;
  else if (c.length() == 7) {
    long v = strtol(c.c_str() + 1, NULL, 16);
    menuBg = C565((v >> 16) & 255, (v >> 8) & 255, v & 255);
    menuBgSet = true;
    bgOn = false;                       // a imagem tem prioridade sobre a cor, entao desliga a imagem
    prefs.putBool("bgi", false);
  }
  prefs.putUShort("mbg", menuBg);
  prefs.putBool("mbs", menuBgSet);
  needRedraw = true;
  server.sendHeader("Location", "/update/adimesp32");
  server.send(302, "text/plain", "");
}

void handleBg() {
  if (blocked(true)) return;
  int o = server.arg("o").toInt();
  String h = server.arg("plain");
  int n = min((int)h.length() / 2, 1024);
  if (o == 0) { bgOn = false; prefs.putBool("bgi", false); LittleFS.remove("/bg.raw"); }
  File f = LittleFS.open("/bg.raw", o == 0 ? "w" : "a");
  if (!f) { server.send(500, "text/plain", "fs"); return; }
  size_t sz = f.size();
  if (o != 0 && sz == (size_t)o + n) { f.close(); server.send(200, "text/plain", "ok"); return; }   // pedaco repetido
  if (sz != (size_t)o) { f.close(); server.send(409, "text/plain", "ordem"); return; }
  static uint8_t buf[1100];
  for (int i = 0; i < n; i++) buf[i] = (hv(h[i * 2]) << 4) | hv(h[i * 2 + 1]);
  f.write(buf, n);
  f.close();
  server.send(200, "text/plain", "ok");
}

void handleBgDone() {
  if (blocked(true)) return;
  File f = LittleFS.open("/bg.raw", "r");
  bool ok = f && f.size() == 153600;
  if (ok) {                                          // confere se nada se corrompeu no envio
    uint32_t a = 1, b = 0;
    static uint8_t cb[256];
    while (f.available()) {
      int n = f.read(cb, sizeof(cb));
      for (int i = 0; i < n; i++) { a = (a + cb[i]) % 65521; b = (b + a) % 65521; }
    }
    uint32_t want = strtoul(server.arg("c").c_str(), NULL, 10);
    ok = (((b << 16) | a) == want);
  }
  if (f) f.close();
  if (!ok) LittleFS.remove("/bg.raw");
  bgOn = ok;
  prefs.putBool("bgi", ok);
  needRedraw = true;
  server.send(200, "text/plain", ok ? "Pronto! Imagem salva." : "Imagem corrompida no envio. Envie de novo.");
  if (ok && server.arg("r") == "1") {
    delay(400);
    showMsg("REINICIANDO...", "aplicando a imagem");
    delay(700);
    ESP.restart();
  }
}

void handleBgOn() {
  if (blocked(true)) return;
  File f = LittleFS.open("/bg.raw", "r");
  bool ok = f && f.size() == 153600;
  if (f) f.close();
  bgOn = ok;
  prefs.putBool("bgi", ok);
  needRedraw = true;
  server.send(200, "text/plain", ok ? "Imagem ativada." : "Nao ha imagem guardada.");
}

void handleReboot() {
  if (blocked(true)) return;
  server.send(200, "text/plain", "ok");
  delay(300);
  showMsg("REINICIANDO...", "aguarde");
  delay(700);
  ESP.restart();
}

void handleBgOff() {
  if (blocked(true)) return;
  bgOn = false;
  prefs.putBool("bgi", false);
  LittleFS.remove("/bg.raw");
  needRedraw = true;
  server.send(200, "text/plain", "ok");
}

void fwUpload() {
  HTTPUpload &u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    fwOk = !needAuth(true) || isAuthed();
    if (!fwOk) return;                       // sem senha: nao grava nada
    showMsg("ATUALIZANDO...", "recebendo o arquivo");
    Update.begin(UPDATE_SIZE_UNKNOWN);
    return;
  }
  if (!fwOk) return;
  if (u.status == UPLOAD_FILE_WRITE) {
    Update.write(u.buf, u.currentSize);
  } else if (u.status == UPLOAD_FILE_END) {
    Update.end(true);
  }
}

void fwDone() {
  if (blocked(true)) return;
  bool ok = !Update.hasError();
  server.send(200, "text/html", ok ? "<meta charset=utf-8><h2>OK! Reiniciando...</h2>" : "<meta charset=utf-8><h2>ERRO na atualizacao. Confira se o arquivo e o .ino.bin</h2><a href=/update/adimesp32>Tentar de novo</a>");
  delay(800);
  if (ok) ESP.restart();
  needRedraw = true;
}

void netInit() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);                 // SO conecta no WiFi do celular. O ESP nao cria WiFi.
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  if (wifiSsid.length()) wifiConnect();

  server.on("/", []() { server.sendHeader("Location", "/esp32"); server.send(302, "text/plain", ""); });
  server.on("/esp32", []() { if (blocked(false)) return; server.send_P(200, "text/html", PAGE); });
  server.on("/login", handleLogin);
  server.on("/logout", handleLogout);
  server.on("/acc", HTTP_POST, handleAccSave);
  server.on("/i", HTTP_GET, handleInput);
  server.on("/t", HTTP_GET, handleTime);
  server.on("/update/adimesp32", handleAdmin);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/bg", HTTP_POST, handleBg);
  server.on("/bgdone", handleBgDone);
  server.on("/bgoff", handleBgOff);
  server.on("/reboot", handleReboot);
  server.on("/bgon", handleBgOn);
  server.on("/fw", HTTP_POST, fwDone, fwUpload);
  static const char *hk[] = {"Cookie"};        // precisa para ler a senha da sessao
  server.collectHeaders(hk, 1);
  server.onNotFound([]() { server.send(404, "text/plain", "nao encontrado"); });
  server.begin();
}

// ---------------- Som e musica ----------------
// Volume 0..5. Duty de 8 bits: quanto menor, mais baixo o som.
const uint8_t volDuty[6] = {0, 3, 8, 20, 50, 128};
int vol = 2;                       // VOLUME PADRAO = 2
uint32_t sndEnd = 0;

void sndInit() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(SND_PIN, 2000, 8);
#else
  ledcSetup(0, 2000, 8);
  ledcAttachPin(SND_PIN, 0);
#endif
}

void toneDuty(int f, int duty) {
  bool off = (f <= 0 || duty <= 0);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  if (off) { ledcWrite(SND_PIN, 0); return; }
  ledcChangeFrequency(SND_PIN, f, 8);
  ledcWrite(SND_PIN, duty);
#else
  if (off) { ledcWrite(0, 0); return; }
  ledcWriteTone(0, f);
  ledcWrite(0, duty);
#endif
}

void sndTone(int f) { toneDuty(f, vol ? (int)volDuty[vol] : 0); }

// Musicas originais: cada caractere e uma nota da escala (0-9), '-' = pausa.
#define NTRACKS 25
const char *TUNE[NTRACKS] = {
  "0247-97420247-9575",  // 0 menu
  "0-2-4-2-0-2-4-7-",    // 1 flappy
  "0-3-0-5-0-3-7-5-",    // 2 cobra
  "04-94-74-92-72-4-",   // 3 blocos
  "02-02-47-47-92-9",    // 4 toca
  "0-4-7-4-0-4-7-9-",    // 5 genius
  "0-0-3-5-0-0-7-5-",    // 6 corrida
  "0-7-0-7-4-9-4-9-",    // 7 pong
  "0---3---5---3---",    // 8 velha
  "0-0-5-0-3-0-7-5-",    // 9 nave
  "0-2-4-7-9-7-4-2-",    // 10 torre
  "02-47-9-74-20-47-",   // 11 2048
  "5-37-5-3-0-35-7-",    // 12 tetris
  "0-3-5-3-0---7-5-",    // 13 labirinto
  "0246-9642-0246-9",    // 14 frutas
  "0-3-0-3-5-3-7-5-",    // 15 carros
  "0-4-2-7-4-9-7-9-",    // 16 memoria
  "0-----3-----5---",    // 17 reflexo
  "0-2-4-2-7-4-9-7-",    // 18 salto
  "0---3-5---0---7-",    // 19 minas
  "07-07-49-49-72-2",    // 20 alvo
  "0-4-7-4-9-7-4-2-",    // 21 liga4
  "0-4-2-5-4-7-5-9-",    // 22 contas
  "0-0-5-7-0-0-9-7-",    // 23 surf
  "0---0---3-3-5---"     // 24 invasores
};
const uint16_t TROOT[NTRACKS] = {262, 330, 220, 294, 392, 262, 247, 330, 262, 196, 349, 294, 330, 220, 392, 262, 440, 330, 392, 196, 523, 294, 330, 294, 196};
const uint8_t TTEMPO[NTRACKS] = {180, 120, 150, 140, 110, 220, 100, 130, 250, 110, 160, 200, 150, 170, 120, 90, 200, 250, 100, 220, 130, 170, 160, 105, 140};
const uint8_t TMIN[NTRACKS]   = {0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 1};
const uint8_t SC[2][10] = {{0, 2, 4, 7, 9, 12, 14, 16, 19, 21}, {0, 3, 5, 7, 10, 12, 15, 17, 19, 22}};
int curTrack = -1, mStep = 0;
uint32_t mNext = 0, mGap = 0;
bool mOn = false;

void musicPlay(int t) {
  curTrack = t; mStep = 0; mNext = millis(); mOn = false;
  if (!sndEnd) toneDuty(0, 0);
}
void musicStop() { curTrack = -1; mOn = false; if (!sndEnd) toneDuty(0, 0); }

void beep(int f, int ms) { sndTone(f); sndEnd = millis() + ms; mOn = false; }

void sndUpdate() {
  uint32_t now = millis();
  if (sndEnd) {                                   // efeito sonoro tocando
    if ((int32_t)(now - sndEnd) < 0) return;
    sndEnd = 0; toneDuty(0, 0); mNext = now;
  }
  if (curTrack < 0 || !musicOn || vol == 0) {
    if (mOn) { toneDuty(0, 0); mOn = false; }
    return;
  }
  if (mOn && (int32_t)(now - mGap) >= 0) { toneDuty(0, 0); mOn = false; }
  if ((int32_t)(now - mNext) >= 0) {
    const char *tn = TUNE[curTrack];
    int n = strlen(tn);
    char c = tn[mStep % n];
    mStep++;
    mNext = now + TTEMPO[curTrack];
    if (c >= '0' && c <= '9') {
      float semi = SC[TMIN[curTrack]][c - '0'];
      int f = (int)(TROOT[curTrack] * powf(2.0f, semi / 12.0f));
      int d = (int)volDuty[vol] / 2;
      if (d < 1) d = 1;
      toneDuty(f, d);
      mOn = true;
      mGap = now + TTEMPO[curTrack] * 7 / 10;
    }
  }
}

void playSeq(const int *f, const int *d, int n) {
  for (int i = 0; i < n; i++) { sndTone(f[i]); delay(d[i]); }
  toneDuty(0, 0); sndEnd = 0; mOn = false;
}

// ---------------- Interface comum ----------------
uint16_t wheel(uint8_t p) {                 // arco-iris
  p = 255 - p;
  if (p < 85)  return tft.color565(255 - p * 3, 0, p * 3);
  if (p < 170) { p -= 85;  return tft.color565(0, p * 3, 255 - p * 3); }
  p -= 170;    return tft.color565(p * 3, 255 - p * 3, 0);
}

void hud(const char *name, int score, int lives) {
  tft.fillRect(0, 0, 320, 20, TFT_NAVY);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString(name, 4, 2, 2);
  tft.setTextColor(TFT_YELLOW, TFT_NAVY);
  tft.drawString("Nv" + String(lvl), 88, 2, 2);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString(String(score) + "/" + String(lvGoal), 128, 2, 2);
  if (lives >= 0) tft.drawString("Vidas:" + String(lives), 214, 2, 2);
  tft.fillRect(298, 0, 22, 20, TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.drawString("X", 305, 2, 2);
}

void startScreen(const char *title, const char *l1, const char *l2) {
  tft.fillScreen(BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(ACC, BG);
  tft.drawString(title, 160, 60, 4);
  tft.setTextColor(FG, BG);
  tft.drawString(l1, 160, 110, 2);
  tft.drawString(l2, 160, 132, 2);
  tft.setTextColor(themeLight ? C565(0, 140, 40) : TFT_GREEN, BG);
  tft.drawString("Toque p/ comecar", 160, 190, 2);
  waitTap();
}

// ---------- escolha da dificuldade (aparece ao iniciar qualquer jogo) ----------
const uint16_t DCOL[4] = {C565(30, 150, 60), C565(40, 110, 220), C565(230, 140, 20), C565(210, 40, 40)};
const char *DSUB[4] = {"mais vidas, mais lento", "como o original", "menos vidas, rapido", "1 vida, bem rapido"};

void drawDiffBtn(int i, bool sel) {
  int x = (i & 1) ? 165 : 10, y = (i >> 1) ? 100 : 34;
  tft.fillRoundRect(x, y, 145, 60, 10, DCOL[i]);
  if (sel) { for (int k = 0; k < 3; k++) tft.drawRoundRect(x - k, y - k, 145 + 2 * k, 60 + 2 * k, 10, TFT_WHITE); }
  else tft.drawRoundRect(x, y, 145, 60, 10, BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, DCOL[i]);
  tft.drawString(DNAME[i], x + 72, y + 24, 4);
  tft.drawString(DSUB[i], x + 72, y + 47, 1);
}

bool pickDiff() {            // true = escolheu, false = voltou ao menu
  int sel = diff;
  tft.fillScreen(BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(ACC2, BG);
  tft.drawString("ESCOLHA A DIFICULDADE", 160, 14, 2);
  for (int i = 0; i < 4; i++) drawDiffBtn(i, i == sel);
  tft.fillRoundRect(100, 176, 120, 40, 10, C565(200, 50, 50));
  tft.setTextColor(TFT_WHITE, C565(200, 50, 50));
  tft.drawString("VOLTAR", 160, 196, 4);
  tft.setTextColor(SUBC, BG);
  tft.drawString("Toque no nivel  (teclado: setas + A)", 160, 228, 2);
  do { netPoll(); readTouch(); delay(10); } while (touched);
  while (true) {
    netPoll(); readTouch(); delay(10);
    if (padE & (K_MENU | K_B)) return false;
    int old = sel;
    if (padE & (K_LEFT | K_RIGHT)) sel ^= 1;
    if (padE & (K_UP | K_DOWN)) sel ^= 2;
    if (sel != old) { drawDiffBtn(old, false); drawDiffBtn(sel, true); beep(700, 15); }
    bool go = (padE & K_A);
    if (pressed) {
      if (ty >= 176 && ty < 216 && tx >= 100 && tx < 220) return false;
      for (int i = 0; i < 4; i++) {
        int x = (i & 1) ? 165 : 10, y = (i >> 1) ? 100 : 34;
        if (tx >= x && tx < x + 145 && ty >= y && ty < y + 60) { sel = i; go = true; }
      }
    }
    if (go) {
      diff = sel;
      prefs.putInt("dif", diff);
      beep(1100, 60);
      return true;
    }
  }
}

void menuDraw();

void endGame(int g, int score) {
  musicStop();
  if (score > hs[g]) {
    hs[g] = score;
    char key[6]; snprintf(key, sizeof(key), "hs%d", g);
    prefs.putInt(key, score);
  }
  if (wonAll) {
    const int f[] = {600, 800, 1000, 1300, 1000, 1300, 1600};
    const int d[] = {110, 110, 110, 160, 90, 90, 350};
    playSeq(f, d, 7);
  } else {
    const int f[] = {500, 400, 300, 200};
    const int d[] = {150, 150, 150, 350};
    playSeq(f, d, 4);
  }

  tft.fillRect(30, 52, 260, 132, BG);
  tft.drawRect(30, 52, 260, 132, FG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(wonAll ? (themeLight ? C565(0, 140, 40) : TFT_GREEN) : TFT_RED, BG);
  tft.drawString(wonAll ? "VOCE ZEROU!" : "FIM DE JOGO", 160, 76, 4);
  tft.setTextColor(FG, BG);
  tft.drawString("Nivel " + String(lvl) + "  (" + String(DNAME[diff]) + ")", 160, 106, 2);
  tft.drawString("Pontos: " + String(score) + "   Recorde: " + String(hs[g]), 160, 128, 2);
  tft.fillRoundRect(42, 150, 112, 28, 8, C565(30, 150, 60));
  tft.fillRoundRect(166, 150, 112, 28, 8, C565(200, 50, 50));
  tft.setTextColor(TFT_WHITE);
  tft.drawString("DE NOVO", 98, 164, 2);
  tft.drawString("MENU", 222, 164, 2);
  wonAll = false;
  bool again = false;
  do { netPoll(); readTouch(); delay(10); } while (touched);
  while (true) {
    netPoll(); readTouch(); delay(10);
    if (padE & K_A) { again = true; break; }
    if (padE & (K_B | K_MENU)) break;
    if (pressed) {
      if (tx >= 42 && tx < 154 && ty >= 150 && ty < 178) { again = true; break; }
      if (tx >= 166 && tx < 278 && ty >= 150 && ty < 178) break;
    }
  }
  if (again) {
    mode = g + 1;
    lvl = 1;
    lvGoal = lvStep(g, 1);
    beep(1000, 40);
    musicPlay(g + 1);
    gameInit(g);
    return;
  }
  mode = MENU;
  menuDraw();
}

// =====================================================
//                    1) FLAPPY
// =====================================================
#define F_BG C565(110, 200, 255)
#define F_PIPE C565(60, 180, 60)
int fgapV = 80;
#define F_GAP fgapV
float fy, fv;
int px[2], pg[2], fscore;
int fspd = 3;
bool fpass[2];

void flappyInit() {
  tft.fillScreen(F_BG);
  fy = 110; fv = 0; fscore = 0;
  fgapV = constrain(80 + (diff == 0 ? 22 : diff == 1 ? 0 : diff == 2 ? -8 : -14) - (lvl - 1) * 2, 54, 110);
  fspd = constrain((int)(3.0f * SPD() + 0.5f), 2, 7);
  px[0] = 320; px[1] = 510;
  pg[0] = random(44, 236 - F_GAP); pg[1] = random(44, 236 - F_GAP);
  fpass[0] = fpass[1] = false;
  hud("FLAPPY", 0, -1);
}

void drawPipe(int i, int speed) {
  int x = px[i], w = 30, gt = pg[i], gb = pg[i] + F_GAP;
  tft.fillRect(x + w, 20, speed, 220, F_BG);
  int x0 = max(x, 0), w0 = min(x + w, 320) - x0;
  if (w0 > 0) {
    tft.fillRect(x0, 20, w0, gt - 20, F_PIPE);
    tft.fillRect(x0, gb, w0, 240 - gb, F_PIPE);
  }
}

void drawBird(int y) {
  tft.fillRect(60, y, 14, 14, TFT_YELLOW);
  tft.fillRect(69, y + 3, 3, 3, TFT_BLACK);
  tft.fillRect(74, y + 7, 4, 3, TFT_ORANGE);
}

void flappyStep() {
  if (pressed) { fv = -5.0; beep(900, 40); }
  tft.fillRect(60, (int)fy, 18, 14, F_BG);
  fv += 0.4; if (fv > 7) fv = 7;
  fy += fv;

  for (int i = 0; i < 2; i++) {
    px[i] -= fspd;
    drawPipe(i, fspd);
    if (px[i] + 30 < 0) {
      px[i] = px[1 - i] + 190;
      if (px[i] < 320) px[i] = 320;
      pg[i] = random(44, 236 - F_GAP);
      fpass[i] = false;
    }
  }
  drawBird((int)fy);

  bool dead = (fy < 20 || fy + 14 > 240);
  for (int i = 0; i < 2; i++) {
    if (60 + 14 > px[i] && 60 < px[i] + 30) {
      if (fy < pg[i] || fy + 14 > pg[i] + F_GAP) dead = true;
    }
    if (!fpass[i] && px[i] + 30 < 60) {
      fpass[i] = true; fscore++;
      hud("FLAPPY", fscore, -1);
      beep(1400, 60);
    }
  }
  if (dead) endGame(0, fscore);
}

// =====================================================
//                    2) COBRA
// =====================================================
#define CS 10
#define GW 32
#define GH 22
#define GY0 20
uint8_t sx[GW * GH], sy[GW * GH];
int slen, sdir, sscore, foodx, foody;
uint32_t snext;
bool sturned;
const int dxs[4] = {0, 1, 0, -1};
const int dys[4] = {-1, 0, 1, 0};

void cell(int x, int y, uint16_t c) {
  tft.fillRect(x * CS + 1, GY0 + y * CS + 1, CS - 2, CS - 2, c);
}

void placeFood() {
  bool ok;
  do {
    foodx = random(0, GW); foody = random(0, GH);
    ok = true;
    for (int i = 0; i < slen; i++) if (sx[i] == foodx && sy[i] == foody) { ok = false; break; }
  } while (!ok);
  cell(foodx, foody, TFT_RED);
}

void cobraInit() {
  tft.fillScreen(TFT_BLACK);
  slen = 3; sdir = 1; sscore = 0; sturned = false;
  for (int i = 0; i < 3; i++) { sx[i] = 16 - i; sy[i] = 11; }
  cell(sx[0], sy[0], TFT_GREENYELLOW);
  cell(sx[1], sy[1], TFT_GREEN);
  cell(sx[2], sy[2], TFT_GREEN);
  hud("COBRA", 0, -1);
  placeFood();
  snext = millis() + 400;
}

void cobraStep() {
  if (pressed && !sturned) {
    sdir = (tx < 160) ? (sdir + 3) & 3 : (sdir + 1) & 3;
    sturned = true;
    beep(500, 20);
  }
  uint32_t now = millis();
  if (now < snext) return;
  snext = now + constrain((int)(170.0f / SPD()), 45, 260);
  sturned = false;

  int nx = sx[0] + dxs[sdir], ny = sy[0] + dys[sdir];
  bool dead = (nx < 0 || nx >= GW || ny < 0 || ny >= GH);
  bool eat = (!dead && nx == foodx && ny == foody);
  if (!dead) {
    int chk = eat ? slen : slen - 1;
    for (int i = 0; i < chk; i++) if (sx[i] == nx && sy[i] == ny) { dead = true; break; }
  }
  if (dead) { endGame(1, sscore); return; }

  if (!eat) cell(sx[slen - 1], sy[slen - 1], TFT_BLACK);
  else if (slen < GW * GH - 1) slen++;
  memmove(&sx[1], &sx[0], slen - 1);
  memmove(&sy[1], &sy[0], slen - 1);
  sx[0] = nx; sy[0] = ny;
  cell(sx[1], sy[1], TFT_GREEN);
  cell(nx, ny, TFT_GREENYELLOW);

  if (eat) {
    sscore++;
    hud("COBRA", sscore, -1);
    beep(1200, 70);
    placeFood();
  }
}

// =====================================================
//                    3) BLOCOS (Breakout)
// =====================================================
#define B_COLS 8
#define B_ROWS 5
#define PADY 222
#define PADW 50
bool br[B_ROWS][B_COLS];
int bleft, bscore, blives, blevel, padx;
float bx, by, bvx, bvy, bspd;
uint32_t bstart;
const uint16_t rowCol[B_ROWS] = {TFT_RED, TFT_ORANGE, TFT_YELLOW, TFT_GREEN, TFT_CYAN};

void drawBricks() {
  for (int r = 0; r < B_ROWS; r++)
    for (int c = 0; c < B_COLS; c++)
      tft.fillRect(c * 40 + 1, 32 + r * 14, 38, 12, br[r][c] ? rowCol[r] : TFT_BLACK);
}

void resetBricks() {
  for (int r = 0; r < B_ROWS; r++) for (int c = 0; c < B_COLS; c++) br[r][c] = true;
  bleft = B_ROWS * B_COLS;
}

void resetBall() {
  bx = padx + PADW / 2; by = PADY - 8;
  bvx = (random(2) ? 1 : -1) * bspd * 0.4;
  bvy = -sqrt(bspd * bspd - bvx * bvx);
  bstart = millis() + 800;
}

void blocosInit() {
  tft.fillScreen(TFT_BLACK);
  bscore = 0; blives = liv(3); blevel = lvl; bspd = min(8.5f, 4.5f * SPD()); padx = 135;
  resetBricks(); drawBricks();
  resetBall();
  hud("BLOCOS", 0, blives);
}

void blocosStep() {
  tft.fillCircle((int)bx, (int)by, 3, TFT_BLACK);

  if (touched) {
    int nx = constrain(tx - PADW / 2, 0, 320 - PADW);
    if (nx != padx) { tft.fillRect(padx, PADY, PADW, 6, TFT_BLACK); padx = nx; }
  }

  bool redrawB = false;
  if (millis() > bstart) {
    bx += bvx; by += bvy;
    if (bx < 3)   { bx = 3;   bvx = -bvx; beep(300, 20); }
    if (bx > 317) { bx = 317; bvx = -bvx; beep(300, 20); }
    if (by < 23)  { by = 23;  bvy = -bvy; beep(300, 20); }

    if (bvy > 0 && by + 3 >= PADY && by - 3 <= PADY + 6 && bx >= padx - 3 && bx <= padx + PADW + 3) {
      float off = (bx - (padx + PADW / 2)) / (PADW / 2.0);
      bvx = off * bspd * 0.8;
      bvy = -sqrt(bspd * bspd - bvx * bvx);
      by = PADY - 4;
      beep(600, 30);
    }

    if (by >= 32 && by < 32 + B_ROWS * 14) {
      int c = (int)bx / 40, r = ((int)by - 32) / 14;
      if (c >= 0 && c < B_COLS && r >= 0 && r < B_ROWS && br[r][c]) {
        br[r][c] = false; bleft--; bscore += 10;
        bvy = -bvy;
        beep(800 + r * 150, 40);
        hud("BLOCOS", bscore, blives);
        redrawB = true;
      }
    }

    if (by > 238) {
      blives--;
      hud("BLOCOS", bscore, blives);
      if (blives <= 0) { endGame(2, bscore); return; }
      beep(200, 200);
      resetBall();
    }

    if (bleft == 0) {
      
      const int f[] = {600, 800, 1000, 1300};
      const int d[] = {100, 100, 100, 200};
      playSeq(f, d, 4);
      resetBricks(); drawBricks(); resetBall();
    }
  }

  if (redrawB || by < 112) drawBricks();
  tft.fillRect(padx, PADY, PADW, 6, TFT_WHITE);
  tft.fillCircle((int)bx, (int)by, 3, TFT_WHITE);
}

// =====================================================
//                    4) TOCA (bate-toupeira)
// =====================================================
#define M_W 106
#define M_H 73
#define M_GRASS C565(40, 150, 50)
#define M_HOLE  C565(40, 25, 10)
#define M_BROWN C565(150, 90, 40)
int mpos[3], mscore, mlives;
bool mon[3];
uint32_t mend[3], mnext;

void drawHole(int c, bool up) {
  int gx = (c % 3) * M_W, gy = 20 + (c / 3) * M_H;
  int cx = gx + M_W / 2, cy = gy + M_H / 2 + 8;
  tft.fillRect(gx, gy, M_W, M_H, M_GRASS);
  tft.fillEllipse(cx, cy + 10, 34, 14, M_HOLE);
  if (up) {
    tft.fillCircle(cx, cy - 4, 22, M_BROWN);
    tft.fillCircle(cx - 8, cy - 10, 5, TFT_WHITE);
    tft.fillCircle(cx + 8, cy - 10, 5, TFT_WHITE);
    tft.fillCircle(cx - 8, cy - 10, 2, TFT_BLACK);
    tft.fillCircle(cx + 8, cy - 10, 2, TFT_BLACK);
    tft.fillCircle(cx, cy, 4, TFT_PINK);
  }
}

bool moleAt(int c) {
  for (int i = 0; i < 3; i++) if (mon[i] && mpos[i] == c) return true;
  return false;
}

void tocaInit() {
  tft.fillScreen(M_GRASS);
  mscore = 0; mlives = liv(3);
  for (int i = 0; i < 3; i++) mon[i] = false;
  for (int c = 0; c < 9; c++) drawHole(c, false);
  hud("TOCA", 0, mlives);
  mnext = millis() + 600;
}

void tocaStep() {
  uint32_t now = millis();

  int cnt = 0;
  for (int i = 0; i < 3; i++) if (mon[i]) cnt++;
  int maxM = min(3, 1 + (lvl - 1) / 3 + (diff >= 2 ? 1 : 0));
  if (now >= mnext && cnt < maxM) {
    int slot = -1;
    for (int i = 0; i < 3; i++) if (!mon[i]) { slot = i; break; }
    int c;
    do { c = random(0, 9); } while (moleAt(c));
    mon[slot] = true; mpos[slot] = c;
    mend[slot] = now + max(380, (int)(1300 / SPD()));
    drawHole(c, true);
    beep(350, 25);
    mnext = now + max(240, (int)(900 / SPD())) + random(0, 300);
  }

  if (pressed && ty >= 20) {
    int c = (tx / M_W) + ((ty - 20) / M_H) * 3;
    if (c > 8) c = 8;
    for (int i = 0; i < 3; i++) {
      if (mon[i] && mpos[i] == c) {
        mon[i] = false; drawHole(c, false);
        mscore++;
        hud("TOCA", mscore, mlives);
        beep(1500, 60);
        break;
      }
    }
  }

  for (int i = 0; i < 3; i++) {
    if (mon[i] && now > mend[i]) {
      mon[i] = false; drawHole(mpos[i], false);
      mlives--;
      hud("TOCA", mscore, mlives);
      beep(150, 150);
      if (mlives <= 0) { endGame(3, mscore); return; }
    }
  }
}

// =====================================================
//                    5) GENIUS (memoria)
// =====================================================
const uint16_t gDim[4] = {C565(110, 20, 20), C565(20, 100, 30), C565(20, 40, 130), C565(120, 110, 20)};
const uint16_t gBri[4] = {C565(255, 60, 60), C565(60, 240, 80), C565(70, 120, 255), C565(255, 235, 60)};
const int gFreq[4] = {330, 415, 523, 659};
uint8_t gseq[60];
int glen, gidx;
bool gplay;

void gPanel(int p, bool on) {
  int gx = (p & 1) * 160, gy = 20 + (p >> 1) * 110;
  tft.fillRoundRect(gx + 4, gy + 4, 152, 102, 14, on ? gBri[p] : gDim[p]);
}

void gFlash(int p, int ms) {
  gPanel(p, true);
  sndTone(gFreq[p]);
  delay(ms);
  sndTone(0);
  gPanel(p, false);
}

void geniusInit() {
  tft.fillScreen(TFT_BLACK);
  for (int p = 0; p < 4; p++) gPanel(p, false);
  hud("GENIUS", 0, -1);
  glen = 0;
  gseq[glen++] = random(4);
  gplay = true;
  gidx = 0;
}

void geniusStep() {
  if (gplay) {
    delay(500);
    int dur = max(95, (int)(max(220, 450 - glen * 12) / SPD()));
    for (int i = 0; i < glen; i++) { gFlash(gseq[i], dur); delay(120); }
    gplay = false; gidx = 0;
    return;
  }
  if (pressed && ty >= 20) {
    int p = (tx >= 160 ? 1 : 0) + (ty >= 130 ? 2 : 0);
    gFlash(p, 180);
    if (p != gseq[gidx]) { endGame(4, glen - 1); return; }
    gidx++;
    if (gidx >= glen) {
      hud("GENIUS", glen, -1);
      if (glen >= 50) { endGame(4, glen); return; }
      gseq[glen++] = random(4);
      gplay = true;
    }
  }
}

// =====================================================
//                    6) CORRIDA (pule os cactos)
// =====================================================
#define R_BG C565(25, 25, 35)
#define R_GROUND 200
float ry, rvy;
int rscore, rframe, roff, nextGap;
int ox[3], ow[3], oh[3];
bool oa[3];
float sinceSp;

void corridaInit() {
  tft.fillScreen(R_BG);
  tft.fillRect(0, R_GROUND, 320, 2, TFT_LIGHTGREY);
  ry = R_GROUND - 22; rvy = 0;
  rscore = 0; rframe = 0; roff = 0;
  for (int i = 0; i < 3; i++) oa[i] = false;
  sinceSp = 0; nextGap = 200;
  hud("CORRIDA", 0, -1);
}

void drawCactus(int i) {
  int x = ox[i], w = ow[i], h = oh[i], y = R_GROUND - h;
  tft.fillRect(x, y, w, h, C565(60, 200, 80));
  tft.fillRect(x + w / 2 - 1, y, 2, h, C565(30, 140, 50));
}

void corridaStep() {
  int spd = constrain((int)(5.0f * SPD() + 0.5f), 3, 12);
  bool onGround = (ry >= R_GROUND - 22);

  if (pressed && onGround) { rvy = -10.5; beep(700, 40); }

  roff = (roff + spd) % 40;
  tft.fillRect(0, R_GROUND + 3, 320, 6, R_BG);
  for (int i = 0; i < 9; i++) tft.fillRect(i * 40 - roff, R_GROUND + 5, 14, 2, TFT_DARKGREY);

  tft.fillRect(40, (int)ry, 16, 22, R_BG);
  rvy += 0.8; ry += rvy;
  if (ry >= R_GROUND - 22) { ry = R_GROUND - 22; rvy = 0; }

  sinceSp += spd;
  if (sinceSp >= nextGap) {
    for (int i = 0; i < 3; i++) {
      if (!oa[i]) {
        oa[i] = true; ox[i] = 320;
        ow[i] = (random(3) == 0) ? 28 : random(12, 17);
        oh[i] = random(20, 36);
        sinceSp = 0;
        nextGap = random(190, 360) + spd * 14;
        break;
      }
    }
  }
  bool dead = false;
  for (int i = 0; i < 3; i++) {
    if (!oa[i]) continue;
    tft.fillRect(ox[i], R_GROUND - oh[i], ow[i], oh[i], R_BG);
    ox[i] -= spd;
    if (ox[i] + ow[i] < 0) { oa[i] = false; continue; }
    drawCactus(i);
    if (42 + 12 > ox[i] && 42 < ox[i] + ow[i] && ry + 22 > R_GROUND - oh[i]) dead = true;
  }

  int yy = (int)ry;
  tft.fillRect(40, yy, 16, 16, TFT_WHITE);
  tft.fillRect(49, yy + 3, 3, 3, TFT_BLACK);
  if (ry < R_GROUND - 22) {
    tft.fillRect(41, yy + 16, 5, 6, TFT_WHITE);
    tft.fillRect(50, yy + 16, 5, 6, TFT_WHITE);
  } else if (rframe & 4) {
    tft.fillRect(41, yy + 16, 5, 6, TFT_WHITE);
  } else {
    tft.fillRect(50, yy + 16, 5, 6, TFT_WHITE);
  }

  rframe++;
  if (rframe % 6 == 0) { rscore++; hud("CORRIDA", rscore, -1); }
  if (dead) endGame(5, rscore);
}

// =====================================================
//                    7) PONG (contra o computador)
// =====================================================
#define P_H 40
float pbx, pby, pbvx, pbvy, cpyf;
int ppy, pscore, plives;
uint32_t pstart;

void pongServe(int dir) {
  pbx = 160; pby = random(60, 200);
  pbvx = dir * min(7.0f, 3.4f * SPD() + 0.6f); pbvy = random(-30, 31) / 10.0;
  pstart = millis() + 700;
}

void pongInit() {
  tft.fillScreen(TFT_BLACK);
  pscore = 0; plives = liv(3);
  ppy = 100; cpyf = 100;
  pongServe(random(2) ? 1 : -1);
  hud("PONG", 0, plives);
}

void pongStep() {
  tft.fillRect((int)pbx - 3, (int)pby - 3, 7, 7, TFT_BLACK);
  for (int y = 24; y < 240; y += 20) tft.fillRect(159, y, 2, 10, TFT_DARKGREY);

  if (touched) {
    int n = constrain(ty - P_H / 2, 20, 240 - P_H);
    if (n != ppy) { tft.fillRect(8, ppy, 6, P_H, TFT_BLACK); ppy = n; }
  }

  float target = (pbvx > 0) ? (pby - P_H / 2) : (130 - P_H / 2);
  float cs = min(6.5f, 2.2f * SPD());
  float d = target - cpyf;
  if (d > cs) d = cs;
  if (d < -cs) d = -cs;
  int oldc = (int)cpyf;
  cpyf = constrain(cpyf + d, 20.0f, (float)(240 - P_H));
  if ((int)cpyf != oldc) tft.fillRect(306, oldc, 6, P_H, TFT_BLACK);

  if (millis() > pstart) {
    pbx += pbvx; pby += pbvy;
    if (pby < 23)  { pby = 23;  pbvy = -pbvy; }
    if (pby > 237) { pby = 237; pbvy = -pbvy; }

    if (pbvx < 0 && pbx - 3 <= 14 && pbx >= 8 && pby >= ppy - 3 && pby <= ppy + P_H + 3) {
      pbx = 18;
      pbvx = min(8.0f, -pbvx * 1.04f);
      pbvy = ((pby - (ppy + P_H / 2)) / (P_H / 2.0f)) * 4.5f;
      beep(700, 25);
    } else if (pbvx > 0 && pbx + 3 >= 306 && pbx <= 312 && pby >= cpyf - 3 && pby <= cpyf + P_H + 3) {
      pbx = 302;
      pbvx = -pbvx;
      pbvy = ((pby - (cpyf + P_H / 2)) / (P_H / 2.0f)) * 4.5f;
      beep(500, 25);
    }

    if (pbx < 0) {
      plives--;
      hud("PONG", pscore, plives);
      beep(200, 250);
      if (plives <= 0) { endGame(6, pscore); return; }
      pongServe(1);
    } else if (pbx > 320) {
      pscore++;
      hud("PONG", pscore, plives);
      beep(1200, 100);
      pongServe(-1);
    }
  }

  tft.fillRect(8, ppy, 6, P_H, TFT_WHITE);
  tft.fillRect(306, (int)cpyf, 6, P_H, TFT_RED);
  tft.fillRect((int)pbx - 3, (int)pby - 3, 6, 6, TFT_WHITE);
}

// =====================================================
//                    8) VELHA (jogo da velha)
// =====================================================
uint8_t vb[9];
int vscore;
const uint8_t lines3[8][3] = {{0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}};

bool vwin(uint8_t p) {
  for (int l = 0; l < 8; l++)
    if (vb[lines3[l][0]] == p && vb[lines3[l][1]] == p && vb[lines3[l][2]] == p) return true;
  return false;
}

bool vfull() {
  for (int i = 0; i < 9; i++) if (vb[i] == 0) return false;
  return true;
}

void vBoard() {
  tft.fillRect(0, 20, 320, 220, TFT_BLACK);
  tft.fillRect(124, 25, 3, 210, TFT_WHITE);
  tft.fillRect(194, 25, 3, 210, TFT_WHITE);
  tft.fillRect(55, 94, 210, 3, TFT_WHITE);
  tft.fillRect(55, 164, 210, 3, TFT_WHITE);
}

void vMark(int i) {
  int cx = 55 + (i % 3) * 70 + 35, cy = 25 + (i / 3) * 70 + 35;
  if (vb[i] == 1) {
    for (int o = -1; o <= 1; o++) {
      tft.drawLine(cx - 20 + o, cy - 20, cx + 20 + o, cy + 20, TFT_CYAN);
      tft.drawLine(cx + 20 + o, cy - 20, cx - 20 + o, cy + 20, TFT_CYAN);
    }
  } else {
    for (int r = 18; r <= 21; r++) tft.drawCircle(cx, cy, r, TFT_RED);
  }
}

void vBanner(const char *s, uint16_t col) {
  tft.fillRoundRect(40, 90, 240, 60, 10, TFT_BLACK);
  tft.drawRoundRect(40, 90, 240, 60, 10, TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(col, TFT_BLACK);
  tft.drawString(s, 160, 120, 4);
  delay(1000);
}

void vNewRound() {
  memset(vb, 0, sizeof(vb));
  vBoard();
}

void velhaInit() {
  tft.fillScreen(TFT_BLACK);
  vscore = 0;
  hud("VELHA", 0, -1);
  vNewRound();
}

int vBlockP() { static const int b[4] = {35, 65, 85, 100}; return min(100, b[diff] + (lvl - 1) * 3); }

int vCpuMove() {
  for (int i = 0; i < 9; i++) {
    if (vb[i]) continue;
    vb[i] = 2; bool w = vwin(2); vb[i] = 0;
    if (w) return i;
  }
  if (random(100) < vBlockP()) {
    for (int i = 0; i < 9; i++) {
      if (vb[i]) continue;
      vb[i] = 1; bool w = vwin(1); vb[i] = 0;
      if (w) return i;
    }
  }
  if (vb[4] == 0 && random(100) < 70) return 4;
  int c;
  do { c = random(9); } while (vb[c]);
  return c;
}

void velhaStep() {
  if (!pressed) return;
  if (tx < 55 || tx >= 265 || ty < 25 || ty >= 235) return;
  int i = ((ty - 25) / 70) * 3 + ((tx - 55) / 70);
  if (i < 0 || i > 8 || vb[i]) return;

  vb[i] = 1; vMark(i); beep(900, 40);
  if (vwin(1)) {
    vscore++;
    hud("VELHA", vscore, -1);
    const int f[] = {800, 1000, 1300};
    const int d[] = {100, 100, 200};
    playSeq(f, d, 3);
    vBanner("VOCE VENCEU!", TFT_GREEN);
    vNewRound();
    return;
  }
  if (vfull()) { vBanner("EMPATE", TFT_YELLOW); vNewRound(); return; }

  delay(350);
  int m = vCpuMove();
  vb[m] = 2; vMark(m); beep(500, 40);
  if (vwin(2)) { delay(500); endGame(7, vscore); return; }
  if (vfull()) { vBanner("EMPATE", TFT_YELLOW); vNewRound(); }
}

// =====================================================
//                    9) NAVE (tiro espacial)
// =====================================================
struct Bul { int x, y; bool a; };
struct Ene { int x, y, sp; bool a; };
Bul nb[8];
Ene ne[6];
int nsx, nscore, nlives;
uint32_t nshot, nspawn;

void naveInit() {
  tft.fillScreen(TFT_BLACK);
  nsx = 160; nscore = 0; nlives = liv(3);
  for (int i = 0; i < 8; i++) nb[i].a = false;
  for (int i = 0; i < 6; i++) ne[i].a = false;
  nshot = nspawn = millis();
  hud("NAVE", 0, nlives);
}

void naveStep() {
  uint32_t now = millis();

  tft.fillRect(nsx - 11, 207, 23, 20, TFT_BLACK);
  if (touched) nsx = constrain(tx, 12, 308);

  if (now - nshot > 260) {
    for (int b = 0; b < 8; b++) {
      if (!nb[b].a) { nb[b].x = nsx; nb[b].y = 204; nb[b].a = true; nshot = now; beep(1800, 12); break; }
    }
  }

  for (int b = 0; b < 8; b++) {
    if (!nb[b].a) continue;
    tft.fillRect(nb[b].x - 1, nb[b].y, 3, 9, TFT_BLACK);
    nb[b].y -= 9;
    if (nb[b].y < 22) nb[b].a = false;
    else tft.fillRect(nb[b].x - 1, nb[b].y, 3, 9, TFT_YELLOW);
  }

  if (now - nspawn > (uint32_t)max(260, (int)(1050 / SPD()))) {
    for (int e = 0; e < 6; e++) {
      if (!ne[e].a) {
        ne[e].x = random(14, 306); ne[e].y = 22;
        ne[e].sp = constrain((int)(2.0f * SPD() + 0.5f), 1, 7);
        ne[e].a = true; nspawn = now;
        break;
      }
    }
  }

  for (int e = 0; e < 6; e++) {
    if (!ne[e].a) continue;
    tft.fillRect(ne[e].x - 9, ne[e].y, 19, 14, TFT_BLACK);
    ne[e].y += ne[e].sp;

    bool shot = false;
    for (int b = 0; b < 8; b++) {
      if (nb[b].a && abs(nb[b].x - ne[e].x) < 11 && nb[b].y < ne[e].y + 14 && nb[b].y + 9 > ne[e].y) {
        nb[b].a = false;
        tft.fillRect(nb[b].x - 1, nb[b].y, 3, 9, TFT_BLACK);
        shot = true;
        break;
      }
    }
    if (shot) {
      ne[e].a = false;
      nscore++;
      hud("NAVE", nscore, nlives);
      beep(1000, 40);
      continue;
    }

    bool hitShip = (ne[e].y + 14 > 208 && ne[e].y < 226 && abs(ne[e].x - nsx) < 19);
    if (hitShip || ne[e].y > 238) {
      ne[e].a = false;
      nlives--;
      hud("NAVE", nscore, nlives);
      beep(150, 200);
      if (nlives <= 0) { endGame(8, nscore); return; }
      continue;
    }

    tft.fillRect(ne[e].x - 9, ne[e].y, 19, 14, TFT_RED);
    tft.fillRect(ne[e].x - 5, ne[e].y + 4, 3, 3, TFT_WHITE);
    tft.fillRect(ne[e].x + 3, ne[e].y + 4, 3, 3, TFT_WHITE);
  }

  tft.fillTriangle(nsx, 208, nsx - 10, 226, nsx + 10, 226, TFT_CYAN);
}

// =====================================================
//                    10) TORRE (empilhar blocos)
// =====================================================
#define T_H 14
int tlx[200], tlw[200];
int tk, tscroll, tcw, tdir;
float tcx, tsp;

uint16_t tcol(int i) { return wheel((uint8_t)(i * 14)); }
int tY(int i) { return 226 - (i - tscroll) * T_H; }
void tDraw(int i) { tft.fillRect(tlx[i], tY(i), tlw[i], T_H - 1, tcol(i)); }

void torreInit() {
  tft.fillScreen(TFT_BLACK);
  tlx[0] = 110; tlw[0] = 100;
  tk = 1; tscroll = 0; tcw = 100;
  tcx = 0; tdir = 1; tsp = 2.4f * SPD();
  tDraw(0);
  hud("TORRE", 0, -1);
}

void torreStep() {
  int y = tY(tk);
  tft.fillRect((int)tcx, y, tcw, T_H - 1, TFT_BLACK);

  if (pressed) {
    int px0 = tlx[tk - 1], pw = tlw[tk - 1];
    int cx = (int)tcx;
    int left = max(cx, px0), right = min(cx + tcw, px0 + pw);
    int ow2 = right - left;
    if (ow2 <= 0) { endGame(9, tk - 1); return; }

    bool perfect = abs(cx - px0) <= (diff == 0 ? 6 : diff == 1 ? 3 : diff == 2 ? 2 : 1);
    if (perfect) { left = px0; ow2 = pw; }
    tlx[tk] = left; tlw[tk] = ow2;
    tDraw(tk);
    beep(perfect ? 1500 : 600 + (tk % 10) * 40, 50);
    hud("TORRE", tk, -1);

    tk++;
    if (tk >= 199) { endGame(9, tk - 1); return; }
    tcw = ow2;
    tsp = min(11.0f, (2.4f + tk * 0.1f) * SPD());
    tdir = (tk & 1) ? 1 : -1;
    tcx = (tdir > 0) ? 0 : (320 - tcw);

    if (tk - tscroll > 12) {
      tscroll++;
      tft.fillRect(0, 20, 320, 220, TFT_BLACK);
      for (int i = tscroll; i < tk; i++) tDraw(i);
    }
    return;
  }

  tcx += tdir * tsp;
  if (tcx <= 0) { tcx = 0; tdir = 1; }
  if (tcx + tcw >= 320) { tcx = 320 - tcw; tdir = -1; }
  tft.fillRect((int)tcx, y, tcw, T_H - 1, tcol(tk));
}

// =====================================================
//                    11) 2048  (NOVO)
//   Toque: deslize o dedo.  Controle: setas.
// =====================================================
#define G2X 54
#define G2Y 24
#define G2C 48
int8_t g2[4][4];
int g2score;
int swx, swy;
bool swOn;
const uint16_t g2col[13] = {
  C565(70, 65, 60), C565(238, 228, 218), C565(237, 224, 200), C565(242, 177, 121),
  C565(245, 149, 99), C565(246, 124, 95), C565(246, 94, 59), C565(237, 207, 114),
  C565(237, 204, 97), C565(237, 200, 80), C565(237, 197, 63), C565(237, 194, 46), C565(60, 58, 50)
};

void g2Add() {
  int cells[16], n = 0;
  for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) if (!g2[r][c]) cells[n++] = r * 4 + c;
  if (!n) return;
  int k = cells[random(n)];
  g2[k / 4][k % 4] = (random(100) >= (diff == 0 ? 5 : diff == 1 ? 10 : diff == 2 ? 20 : 30)) ? 1 : 2;
}

void g2Draw() {
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      int x = G2X + 4 + c * (G2C + 4), y = G2Y + 4 + r * (G2C + 4);
      int e = g2[r][c];
      uint16_t col = g2col[min(e, 12)];
      tft.fillRoundRect(x, y, G2C, G2C, 4, col);
      if (e) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(e <= 2 ? C565(90, 80, 70) : TFT_WHITE, col);
        tft.drawString(String(1L << e), x + G2C / 2, y + G2C / 2 + 1, 2);
      }
    }
  }
}

bool g2Move(int d) {
  bool moved = false;
  for (int i = 0; i < 4; i++) {
    int8_t line[4], out[4] = {0, 0, 0, 0};
    int rr[4], cc[4];
    for (int j = 0; j < 4; j++) {
      int r, c;
      switch (d) {
        case 0:  r = j;     c = i;     break;
        case 1:  r = i;     c = 3 - j; break;
        case 2:  r = 3 - j; c = i;     break;
        default: r = i;     c = j;     break;
      }
      rr[j] = r; cc[j] = c; line[j] = g2[r][c];
    }
    int k = 0; bool cm = false;
    for (int j = 0; j < 4; j++) {
      int8_t v = line[j];
      if (!v) continue;
      if (cm && out[k - 1] == v) { out[k - 1]++; g2score += 1 << out[k - 1]; cm = false; }
      else { out[k++] = v; cm = true; }
    }
    for (int j = 0; j < 4; j++) {
      if (g2[rr[j]][cc[j]] != out[j]) { moved = true; g2[rr[j]][cc[j]] = out[j]; }
    }
  }
  return moved;
}

bool g2Can() {
  for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) {
    if (!g2[r][c]) return true;
    if (c < 3 && g2[r][c] == g2[r][c + 1]) return true;
    if (r < 3 && g2[r][c] == g2[r + 1][c]) return true;
  }
  return false;
}

void g2Init() {
  tft.fillScreen(TFT_BLACK);
  memset(g2, 0, sizeof(g2));
  g2score = 0; swOn = false;
  tft.fillRoundRect(G2X, G2Y, 212, 212, 6, C565(120, 110, 100));
  g2Add(); g2Add();
  g2Draw();
  hud("2048", 0, -1);
}

void g2Step() {
  int dir = -1;
  if (padE & K_UP) dir = 0;
  else if (padE & K_RIGHT) dir = 1;
  else if (padE & K_DOWN) dir = 2;
  else if (padE & K_LEFT) dir = 3;

  if (pressed) { swx = tx; swy = ty; swOn = true; }
  if (swOn && touched) {
    int dx = tx - swx, dy = ty - swy;
    if (abs(dx) > 28 || abs(dy) > 28) {
      dir = (abs(dx) > abs(dy)) ? (dx > 0 ? 1 : 3) : (dy > 0 ? 2 : 0);
      swOn = false;
    }
  }
  if (!touched) swOn = false;

  if (dir >= 0 && g2Move(dir)) {
    g2Add();
    g2Draw();
    hud("2048", g2score, -1);
    beep(700, 30);
    if (!g2Can()) endGame(10, g2score);
  }
}

// =====================================================
//                    12) TETRIS  (NOVO)
//   Toque: esq/dir mover, centro girar, centro embaixo descer.
//   Controle: setas, A/cima gira, B queda rapida.
// =====================================================
#define TX0 105
#define TC 11
const uint16_t TET[7][4] = {
  {0x0F00, 0x2222, 0x00F0, 0x4444},   // I
  {0x8E00, 0x6440, 0x0E20, 0x44C0},   // J
  {0x2E00, 0x4460, 0x0E80, 0xC440},   // L
  {0x6600, 0x6600, 0x6600, 0x6600},   // O
  {0x6C00, 0x4620, 0x06C0, 0x8C40},   // S
  {0x4E00, 0x4640, 0x0E40, 0x4C40},   // T
  {0xC600, 0x2640, 0x0C60, 0x4C80}    // Z
};
const uint16_t tcols[7] = {TFT_CYAN, TFT_BLUE, TFT_ORANGE, TFT_YELLOW, TFT_GREEN, TFT_MAGENTA, TFT_RED};
uint8_t tbd[20][10];
int tcur, trot, tpx, tpy, tnxt, tscore, tlines;
uint32_t tfallT, trep;

bool tFits(int p, int r, int x, int y) {
  uint16_t m = TET[p][r];
  for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) {
    if (m & (0x8000 >> (i * 4 + j))) {
      int cx = x + j, cy = y + i;
      if (cx < 0 || cx >= 10 || cy >= 20) return false;
      if (cy >= 0 && tbd[cy][cx]) return false;
    }
  }
  return true;
}

void tCell(int x, int y, uint16_t c) { tft.fillRect(TX0 + x * TC, 20 + y * TC, TC - 1, TC - 1, c); }

void tDrawAll() {
  for (int y = 0; y < 20; y++) for (int x = 0; x < 10; x++)
    tCell(x, y, tbd[y][x] ? tcols[tbd[y][x] - 1] : C565(15, 15, 25));
  uint16_t m = TET[tcur][trot];
  for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
    if ((m & (0x8000 >> (i * 4 + j))) && tpy + i >= 0) tCell(tpx + j, tpy + i, tcols[tcur]);
}

void tSide() {
  tft.fillRect(0, 20, TX0 - 2, 220, TFT_BLACK);
  tft.fillRect(TX0 + 10 * TC + 2, 20, 320 - (TX0 + 10 * TC + 2), 220, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("PROX", 14, 30, 2);
  uint16_t m = TET[tnxt][0];
  for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
    if (m & (0x8000 >> (i * 4 + j))) tft.fillRect(20 + j * 11, 56 + i * 11, 10, 10, tcols[tnxt]);
  tft.drawString("LINHAS", 226, 30, 2);
  tft.drawString(String(tlines), 226, 50, 2);
  tft.drawString("NIVEL", 226, 90, 2);
  tft.drawString(String(lvl), 226, 110, 2);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("<", 40, 190, 4);
  tft.drawString(">", 250, 190, 4);
}

bool tSpawn() {
  tcur = tnxt; tnxt = random(7);
  trot = 0; tpx = 3; tpy = 0;
  if (!tFits(tcur, trot, tpx, tpy)) { endGame(11, tscore); return false; }
  return true;
}

bool tLockNext() {                       // trava a peca, limpa linhas, cria a proxima
  uint16_t m = TET[tcur][trot];
  for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++)
    if (m & (0x8000 >> (i * 4 + j))) {
      int cx = tpx + j, cy = tpy + i;
      if (cy >= 0) tbd[cy][cx] = tcur + 1;
    }
  int n = 0;
  for (int y = 19; y >= 0; y--) {
    bool full = true;
    for (int x = 0; x < 10; x++) if (!tbd[y][x]) { full = false; break; }
    if (full) {
      n++;
      for (int yy = y; yy > 0; yy--) memcpy(tbd[yy], tbd[yy - 1], 10);
      memset(tbd[0], 0, 10);
      y++;
    }
  }
  if (n) {
    static const int pts[5] = {0, 100, 300, 500, 800};
    tscore += pts[n] * lvl;
    tlines += n;
    hud("TETRIS", tscore, -1);
    beep(1000 + n * 150, 120);
  } else beep(300, 30);
  if (!tSpawn()) return false;
  tSide();
  return true;
}

void tetrisInit() {
  tft.fillScreen(TFT_BLACK);
  memset(tbd, 0, sizeof(tbd));
  tscore = 0; tlines = 0; tnxt = random(7);
  hud("TETRIS", 0, -1);
  tft.drawRect(TX0 - 2, 19, TC * 10 + 3, 222, TFT_WHITE);
  tSpawn();
  tSide();
  tDrawAll();
  tfallT = millis() + 600; trep = 0;
}

void tetrisStep() {
  uint32_t now = millis();
  bool ch = false;

  int held = 0;
  if ((touched && tx < 107) || (padHeld & K_LEFT)) held = -1;
  else if ((touched && tx > 213) || (padHeld & K_RIGHT)) held = 1;
  bool np = (pressed && (tx < 107 || tx > 213)) || (padE & (K_LEFT | K_RIGHT));
  if (held) {
    if (np) {
      if (tFits(tcur, trot, tpx + held, tpy)) { tpx += held; ch = true; }
      trep = now + 220;
    } else if (now >= trep) {
      if (tFits(tcur, trot, tpx + held, tpy)) { tpx += held; ch = true; }
      trep = now + 80;
    }
  }

  bool rot = (padE & (K_UP | K_A)) || (pressed && tx >= 107 && tx <= 213 && ty < 160);
  if (rot) {
    int nr = (trot + 1) & 3;
    if (tFits(tcur, nr, tpx, tpy)) { trot = nr; ch = true; }
    else if (tFits(tcur, nr, tpx - 1, tpy)) { trot = nr; tpx--; ch = true; }
    else if (tFits(tcur, nr, tpx + 1, tpy)) { trot = nr; tpx++; ch = true; }
    beep(700, 15);
  }

  bool soft = (padHeld & K_DOWN) || (touched && tx >= 107 && tx <= 213 && ty >= 160);
  uint32_t iv = soft ? 50 : (uint32_t)max(70, (int)(600 / SPD()));

  if (padE & K_B) {                                   // queda rapida
    while (tFits(tcur, trot, tpx, tpy + 1)) tpy++;
    if (!tLockNext()) return;
    tfallT = now + iv;
    tDrawAll();
    return;
  }

  if (now >= tfallT) {
    tfallT = now + iv;
    if (tFits(tcur, trot, tpx, tpy + 1)) { tpy++; ch = true; }
    else { if (!tLockNext()) return; ch = true; }
  }
  if (ch) tDrawAll();
}

// =====================================================
//                    13) LABIRINTO  (NOVO)
//   Toque: toque na direcao que quer ir.  Controle: setas.
// =====================================================
#define LB_R 11
#define LB_C 16
uint8_t mz[LB_R][LB_C];
int lbr, lbc, lbscore, lblevel, lbsec;
uint32_t lbend, lbrep;
const int ldr[4] = {-1, 0, 1, 0};
const int ldc[4] = {0, 1, 0, -1};

void lbGen() {
  memset(mz, 0, sizeof(mz));
  bool vis[LB_R][LB_C] = {};
  int stk[LB_R * LB_C][2], sp = 0;
  vis[0][0] = true; stk[sp][0] = 0; stk[sp][1] = 0; sp++;
  while (sp) {
    int r = stk[sp - 1][0], c = stk[sp - 1][1];
    int opts[4], n = 0;
    for (int d = 0; d < 4; d++) {
      int nr = r + ldr[d], nc = c + ldc[d];
      if (nr >= 0 && nr < LB_R && nc >= 0 && nc < LB_C && !vis[nr][nc]) opts[n++] = d;
    }
    if (!n) { sp--; continue; }
    int d = opts[random(n)];
    int nr = r + ldr[d], nc = c + ldc[d];
    mz[r][c] |= (1 << d);
    mz[nr][nc] |= (1 << ((d + 2) & 3));
    vis[nr][nc] = true;
    stk[sp][0] = nr; stk[sp][1] = nc; sp++;
  }
}

void lbCell(int r, int c) {
  int x = c * 20, y = 20 + r * 20;
  uint16_t w = C565(80, 120, 255);
  tft.fillRect(x, y, 20, 20, TFT_BLACK);
  if (!(mz[r][c] & 1)) tft.fillRect(x, y, 20, 2, w);
  if (!(mz[r][c] & 8)) tft.fillRect(x, y, 2, 20, w);
  if (!(mz[r][c] & 2)) tft.fillRect(x + 18, y, 2, 20, w);
  if (!(mz[r][c] & 4)) tft.fillRect(x, y + 18, 20, 2, w);
  if (r == LB_R - 1 && c == LB_C - 1) tft.fillRect(x + 6, y + 6, 8, 8, TFT_GREEN);
}

void lbPlayer() { tft.fillCircle(lbc * 20 + 10, 20 + lbr * 20 + 10, 5, TFT_YELLOW); }

void lbNew() {
  lbGen();
  lbr = 0; lbc = 0;
  for (int r = 0; r < LB_R; r++) for (int c = 0; c < LB_C; c++) lbCell(r, c);
  lbPlayer();
  lbend = millis() + (uint32_t)max(18, (diff == 0 ? 80 : diff == 1 ? 62 : diff == 2 ? 50 : 40) - (lvl - 1) * 3 - lblevel) * 1000UL;
  lbsec = -1;
}

void labInit() {
  tft.fillScreen(TFT_BLACK);
  lbscore = 0; lblevel = 0; lbrep = 0;
  hud("LABIRINTO", 0, -1);
  lbNew();
}

void labStep() {
  uint32_t now = millis();
  int want = -1;
  if (padHeld & K_UP) want = 0;
  else if (padHeld & K_RIGHT) want = 1;
  else if (padHeld & K_DOWN) want = 2;
  else if (padHeld & K_LEFT) want = 3;
  else if (touched) {
    int dx = tx - (lbc * 20 + 10), dy = ty - (20 + lbr * 20 + 10);
    if (abs(dx) > 6 || abs(dy) > 6) want = (abs(dx) > abs(dy)) ? (dx > 0 ? 1 : 3) : (dy > 0 ? 2 : 0);
  }
  bool np = pressed || (padE & (K_UP | K_DOWN | K_LEFT | K_RIGHT));
  if (want >= 0 && (np || now >= lbrep)) {
    lbrep = now + (np ? 220 : 110);
    if (mz[lbr][lbc] & (1 << want)) {
      lbCell(lbr, lbc);
      lbr += ldr[want]; lbc += ldc[want];
      lbPlayer();
      beep(500, 10);
      if (lbr == LB_R - 1 && lbc == LB_C - 1) {
        lbscore++; lblevel++;
        hud("LABIRINTO", lbscore, -1);
        const int f[] = {800, 1000, 1300};
        const int d[] = {80, 80, 160};
        playSeq(f, d, 3);
        lbNew();
        return;
      }
    }
  }
  int sec = (lbend > now) ? (int)((lbend - now) / 1000) : 0;
  if (sec != lbsec) {
    lbsec = sec;
    tft.fillRect(214, 2, 80, 16, TFT_NAVY);
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("Tempo: " + String(sec), 214, 2, 2);
  }
  if (now >= lbend) endGame(12, lbscore);
}

// =====================================================
//                    14) FRUTAS  (NOVO)
//   Arraste a cesta (ou setas). Pegue frutas, fuja das bombas.
// =====================================================
struct Frt { int x, y, sp, t; uint16_t col; bool a; };
Frt frt[6];
int frx, frsc, frlv;
uint32_t frsp;
const uint16_t frCols[4] = {TFT_RED, TFT_ORANGE, TFT_YELLOW, TFT_MAGENTA};

void frInit() {
  tft.fillScreen(TFT_BLACK);
  frx = 160; frsc = 0; frlv = liv(3);
  for (int i = 0; i < 6; i++) frt[i].a = false;
  frsp = millis() + 500;
  hud("FRUTAS", 0, frlv);
}

bool frLose() {                          // true = acabou o jogo
  frlv--;
  hud("FRUTAS", frsc, frlv);
  beep(150, 200);
  if (frlv <= 0) { endGame(13, frsc); return true; }
  return false;
}

void frStep() {
  uint32_t now = millis();
  tft.fillRect(frx - 23, 222, 47, 10, TFT_BLACK);
  if (touched) frx = constrain(tx, 24, 295);

  if (now >= frsp) {
    for (int i = 0; i < 6; i++) {
      if (!frt[i].a) {
        frt[i].x = random(14, 306); frt[i].y = 32;
        frt[i].sp = constrain((int)(2.0f * SPD() + 0.5f), 1, 7);
        frt[i].t = (random(100) < (diff == 0 ? 10 : diff == 1 ? 18 : diff == 2 ? 26 : 34)) ? 1 : 0;
        frt[i].col = frCols[random(4)];
        frt[i].a = true;
        frsp = now + max(260, (int)(900 / SPD()));
        break;
      }
    }
  }

  for (int i = 0; i < 6; i++) {
    if (!frt[i].a) continue;
    tft.fillCircle(frt[i].x, frt[i].y, 11, TFT_BLACK);
    frt[i].y += frt[i].sp;

    bool caught = (frt[i].y + 8 >= 222 && frt[i].y - 8 <= 232 && abs(frt[i].x - frx) < 26);
    if (caught) {
      frt[i].a = false;
      if (frt[i].t) { if (frLose()) return; }
      else { frsc++; hud("FRUTAS", frsc, frlv); beep(1200, 40); }
      continue;
    }
    if (frt[i].y > 236) {
      frt[i].a = false;
      if (!frt[i].t) { if (frLose()) return; }
      continue;
    }
    if (frt[i].t) {
      tft.fillCircle(frt[i].x, frt[i].y, 8, C565(60, 60, 60));
      tft.fillCircle(frt[i].x + 3, frt[i].y - 9, 2, TFT_ORANGE);
    } else {
      tft.fillCircle(frt[i].x, frt[i].y, 7, frt[i].col);
      tft.fillRect(frt[i].x - 1, frt[i].y - 11, 2, 4, TFT_GREEN);
    }
  }

  tft.fillRect(frx - 20, 224, 41, 8, C565(160, 110, 50));
  tft.fillRect(frx - 22, 222, 45, 3, C565(200, 150, 80));
}

// =====================================================
//                    15) CARROS  (NOVO)
//   Toque esq/dir (ou setas) para trocar de faixa.
// =====================================================
#define ROAD_C C565(60, 60, 60)
const int laneX[3] = {100, 160, 220};
struct Car { int lane, y, sp; uint16_t col; bool a; };
Car cars[4];
int cplane, csc, coff;
uint32_t cspawn;
const uint16_t carCols[4] = {TFT_YELLOW, TFT_ORANGE, TFT_MAGENTA, TFT_CYAN};

void carDraw(int cx, int y, uint16_t col) {
  uint16_t gl = C565(30, 60, 110);
  tft.fillRect(cx - 13, y, 26, 44, col);
  tft.fillRect(cx - 9, y + 9, 18, 9, gl);
  tft.fillRect(cx - 9, y + 28, 18, 7, gl);
  tft.fillRect(cx - 16, y + 5, 3, 11, TFT_BLACK);
  tft.fillRect(cx + 13, y + 5, 3, 11, TFT_BLACK);
  tft.fillRect(cx - 16, y + 28, 3, 11, TFT_BLACK);
  tft.fillRect(cx + 13, y + 28, 3, 11, TFT_BLACK);
}

void carErase(int cx, int y) { tft.fillRect(cx - 16, y, 33, 44, ROAD_C); }

void carInit() {
  tft.fillScreen(C565(30, 130, 40));
  tft.fillRect(70, 20, 180, 220, ROAD_C);
  tft.fillRect(67, 20, 3, 220, TFT_WHITE);
  tft.fillRect(250, 20, 3, 220, TFT_WHITE);
  cplane = 1; csc = 0; coff = 0;
  for (int i = 0; i < 4; i++) cars[i].a = false;
  cspawn = millis() + 600;
  hud("CARROS", 0, -1);
}

void carStep() {
  uint32_t now = millis();
  int ol = cplane;
  if (pressed) {
    if (tx < 160) cplane = max(0, cplane - 1);
    else cplane = min(2, cplane + 1);
    beep(600, 15);
  }
  if (ol != cplane) carErase(laneX[ol], 190);

  int spd = constrain((int)(3.0f * SPD() + 0.5f), 2, 9);
  coff += spd;

  if (now >= cspawn) {
    int ln = random(3);
    bool ok = true;
    for (int i = 0; i < 4; i++) if (cars[i].a && cars[i].lane == ln && cars[i].y < 90) ok = false;
    if (ok) {
      for (int i = 0; i < 4; i++) {
        if (!cars[i].a) {
          cars[i].lane = ln; cars[i].y = 20; cars[i].col = carCols[random(4)]; cars[i].a = true;
          break;
        }
      }
    }
    cspawn = now + (uint32_t)max(380, (int)(1100 / SPD()));
  }

  // faixas tracejadas
  for (int d = 0; d < 2; d++) {
    int x = 130 + d * 60;
    tft.fillRect(x - 2, 20, 4, 220, ROAD_C);
    for (int y = 20 + (coff % 40) - 40; y < 240; y += 40) {
      int y0 = max(y, 20), h = min(y + 20, 240) - y0;
      if (h > 0) tft.fillRect(x - 1, y0, 3, h, TFT_WHITE);
    }
  }

  bool dead = false;
  for (int i = 0; i < 4; i++) {
    if (!cars[i].a) continue;
    int cx = laneX[cars[i].lane];
    carErase(cx, cars[i].y);
    cars[i].y += spd;
    if (cars[i].y > 238) {
      cars[i].a = false;
      csc++;
      hud("CARROS", csc, -1);
      beep(900, 30);
      continue;
    }
    carDraw(cx, cars[i].y, cars[i].col);
    if (cars[i].lane == cplane && cars[i].y + 44 > 194 && cars[i].y < 234) dead = true;
  }
  carDraw(laneX[cplane], 190, TFT_RED);
  if (dead) endGame(14, csc);
}

// =====================================================
//                    NOVOS JOGOS 16..21
// =====================================================

// ---------- 16) MEMORIA ----------
#define MC_X 20
#define MC_Y 28
#define MC_W 64
#define MC_H 46
uint8_t mmc[16], mmo[16];
int mmA, mmScore, mmLives;
const uint16_t mmCol[8] = {TFT_RED, TFT_BLUE, TFT_GREEN, TFT_ORANGE, TFT_MAGENTA, TFT_CYAN, C565(230, 190, 0), C565(255, 105, 180)};

void mmDraw(int i) {
  int x = MC_X + (i % 4) * (MC_W + 8), y = MC_Y + (i / 4) * (MC_H + 6);
  if (mmo[i] == 0) {
    uint16_t c0 = C565(60, 60, 190);
    tft.fillRoundRect(x, y, MC_W, MC_H, 6, c0);
    tft.drawRoundRect(x, y, MC_W, MC_H, 6, TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, c0);
    tft.drawString("?", x + MC_W / 2, y + MC_H / 2, 4);
  } else {
    uint16_t bg = (mmo[i] == 2) ? C565(40, 90, 50) : TFT_WHITE;
    tft.fillRoundRect(x, y, MC_W, MC_H, 6, bg);
    int cx = x + MC_W / 2, cy = y + MC_H / 2, s = mmc[i];
    uint16_t c = mmCol[s];
    switch (s % 4) {
      case 0: tft.fillCircle(cx, cy, 15, c); break;
      case 1: tft.fillRect(cx - 14, cy - 14, 28, 28, c); break;
      case 2: tft.fillTriangle(cx, cy - 16, cx - 16, cy + 14, cx + 16, cy + 14, c); break;
      default:
        tft.fillTriangle(cx, cy - 17, cx - 15, cy, cx + 15, cy, c);
        tft.fillTriangle(cx, cy + 17, cx - 15, cy, cx + 15, cy, c);
        break;
    }
  }
}

void mmNew() {
  uint8_t t[16];
  for (int i = 0; i < 16; i++) t[i] = i / 2;
  for (int i = 15; i > 0; i--) { int j = random(i + 1); uint8_t x = t[i]; t[i] = t[j]; t[j] = x; }
  for (int i = 0; i < 16; i++) { mmc[i] = t[i]; mmo[i] = 0; }
  mmA = -1;
  tft.fillRect(0, 20, 320, 220, TFT_BLACK);
  for (int i = 0; i < 16; i++) mmDraw(i);
}

void mmInit() {
  tft.fillScreen(TFT_BLACK);
  mmScore = 0; mmLives = liv(6);
  hud("MEMORIA", 0, mmLives);
  mmNew();
}

void mmStep() {
  if (!pressed || ty < MC_Y || tx < MC_X) return;
  int col = (tx - MC_X) / (MC_W + 8), row = (ty - MC_Y) / (MC_H + 6);
  if (col > 3 || row > 3) return;
  int i = row * 4 + col;
  if (mmo[i] != 0) return;
  mmo[i] = 1; mmDraw(i); beep(600 + mmc[i] * 80, 40);
  if (mmA < 0) { mmA = i; return; }
  int a = mmA; mmA = -1;
  if (mmc[a] == mmc[i]) {
    mmo[a] = 2; mmo[i] = 2; mmDraw(a); mmDraw(i);
    mmScore++;
    hud("MEMORIA", mmScore, mmLives);
    beep(1500, 80);
    bool done = true;
    for (int k = 0; k < 16; k++) if (mmo[k] != 2) done = false;
    if (done) {
      mmScore += 5; mmLives = min(9, mmLives + 2);
      hud("MEMORIA", mmScore, mmLives);
      const int f[] = {800, 1000, 1300};
      const int d[] = {100, 100, 200};
      playSeq(f, d, 3);
      mmNew();
    }
  } else {
    delay(diff == 0 ? 950 : diff == 1 ? 650 : diff == 2 ? 480 : 340);
    mmo[a] = 0; mmo[i] = 0; mmDraw(a); mmDraw(i);
    mmLives--;
    hud("MEMORIA", mmScore, mmLives);
    beep(150, 150);
    if (mmLives <= 0) endGame(15, mmScore);
  }
}

// ---------- 17) REFLEXO ----------
int rfSt, rfLives, rfScore;
uint32_t rfT0, rfGo;

void rfScr(uint16_t col, String a, String b) {
  tft.fillRect(0, 20, 320, 220, col);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, col);
  tft.drawString(a, 160, 110, 4);
  tft.drawString(b, 160, 150, 2);
}

void rfNew() {
  rfScr(C565(200, 30, 30), "ESPERE...", "toque so quando ficar verde");
  rfGo = millis() + random(1200, 3800);
  rfSt = 1;
}

void rfInit() {
  tft.fillScreen(TFT_BLACK);
  rfLives = liv(3); rfScore = 0;
  hud("REFLEXO", 0, rfLives);
  rfNew();
}

void rfLose(const char *why) {
  rfLives--;
  hud("REFLEXO", rfScore, rfLives);
  beep(150, 250);
  rfScr(C565(220, 150, 0), why, "");
  delay(1000);
  if (rfLives <= 0) { endGame(16, rfScore); return; }
  rfNew();
}

void rfStep() {
  uint32_t now = millis();
  if (rfSt == 1) {
    if (pressed) { rfLose("CEDO DEMAIS!"); return; }
    if (now >= rfGo) { rfScr(C565(30, 190, 60), "TOQUE!", ""); rfT0 = now; rfSt = 2; beep(1100, 40); }
  } else if (rfSt == 2) {
    if (pressed) {
      int ms = (int)(now - rfT0);
      int pts = max(1, (700 - ms) / 10);
      rfScore += pts;
      hud("REFLEXO", rfScore, rfLives);
      rfScr(C565(30, 90, 200), String(ms) + " ms", "+" + String(pts) + " pontos");
      beep(1400, 80);
      delay(1000);
      rfNew();
    } else if (now - rfT0 > (uint32_t)max(450, (diff == 0 ? 1700 : diff == 1 ? 1200 : diff == 2 ? 950 : 750) - (lvl - 1) * 40)) rfLose("MUITO LENTO!");
  }
}

// ---------- 18) SALTO ----------
#define SJ_BG C565(110, 195, 255)
float sjx, sjy, sjv, sjh;
int sjpx[7], sjpy[7], sjscore, sjw = 40, sjbase = 0;

void sjInit() {
  tft.fillScreen(SJ_BG);
  sjx = 153; sjy = 190; sjv = -9.5; sjh = 0; sjscore = 0; sjbase = 0;
  sjw = constrain((diff == 0 ? 52 : diff == 1 ? 40 : diff == 2 ? 34 : 28) - (lvl - 1), 22, 56);
  for (int i = 0; i < 7; i++) { sjpx[i] = random(0, 320 - sjw); sjpy[i] = 215 - i * 33; }
  sjpx[0] = 140;
  hud("SALTO", 0, -1);
}

void sjStep() {
  tft.fillRect((int)sjx, (int)sjy, 14, 14, SJ_BG);
  for (int i = 0; i < 7; i++) if (sjpy[i] >= 22 && sjpy[i] < 238) tft.fillRect(sjpx[i], sjpy[i], sjw, 6, SJ_BG);

  if (touched) {
    float d = tx - (sjx + 7);
    if (d > 6) d = 6;
    if (d < -6) d = -6;
    sjx += d;
  }
  if (sjx < -14) sjx = 319;
  else if (sjx > 319) sjx = -14;

  sjv += 0.4; sjy += sjv;
  if (sjv > 0) {
    for (int i = 0; i < 7; i++) {
      if (sjy + 14 >= sjpy[i] && sjy + 14 <= sjpy[i] + 6 + sjv && sjx + 14 > sjpx[i] && sjx < sjpx[i] + sjw) {
        sjv = -9.5; beep(800, 30);
        break;
      }
    }
  }
  if (sjy < 110) {
    float dy = 110 - sjy;
    sjy = 110;
    sjh += dy;
    int di = (int)(dy + 0.5f);
    for (int i = 0; i < 7; i++) sjpy[i] += di;
    for (int i = 0; i < 7; i++) {
      if (sjpy[i] > 238) {
        int mn = 999;
        for (int j = 0; j < 7; j++) if (sjpy[j] < mn) mn = sjpy[j];
        sjpy[i] = mn - random(34 + 3 * diff, 56 + 4 * diff + lvl);
        sjpx[i] = random(0, 320 - sjw);
      }
    }
    int ns = sjbase + (int)(sjh / 10);
    if (ns != sjscore) { sjscore = ns; hud("SALTO", sjscore, -1); }
  }
  if (sjy > 240) { endGame(17, sjscore); return; }

  for (int i = 0; i < 7; i++) if (sjpy[i] >= 22 && sjpy[i] < 238) tft.fillRect(sjpx[i], sjpy[i], sjw, 6, C565(40, 170, 50));
  tft.fillRect((int)sjx, (int)sjy, 14, 14, TFT_YELLOW);
  tft.fillRect((int)sjx + 8, (int)sjy + 3, 3, 3, TFT_BLACK);
}

// ---------- 19) MINAS ----------
#define MN_R 8
#define MN_C 11
#define MN_S 25
#define MN_X 22
#define MN_Y 26
int mnMinesV = 10;
#define MN_MINES mnMinesV
uint8_t mnm[MN_R][MN_C], mnr[MN_R][MN_C], mnn[MN_R][MN_C];
int mnscore, mnrev, mnCr, mnCc;
bool mnFirst, mnDown, mnFired;
uint32_t mnT0;
const uint16_t mnNumCol[9] = {0, TFT_BLUE, C565(0, 130, 0), TFT_RED, C565(0, 0, 130), C565(130, 0, 0), C565(0, 130, 130), TFT_BLACK, TFT_DARKGREY};

void mnDraw(int r, int c) {
  int x = MN_X + c * MN_S, y = MN_Y + r * MN_S;
  uint16_t lite = C565(225, 225, 235);
  if (mnr[r][c] != 1) {
    tft.fillRect(x, y, MN_S - 1, MN_S - 1, C565(90, 110, 210));
    tft.drawRect(x, y, MN_S - 1, MN_S - 1, C565(150, 170, 255));
    if (mnr[r][c] == 2) {
      tft.fillRect(x + 7, y + 4, 2, 16, TFT_WHITE);
      tft.fillTriangle(x + 9, y + 4, x + 9, y + 13, x + 19, y + 8, TFT_RED);
    }
  } else {
    tft.fillRect(x, y, MN_S - 1, MN_S - 1, lite);
    if (mnm[r][c]) tft.fillCircle(x + 12, y + 12, 7, TFT_BLACK);
    else if (mnn[r][c]) {
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(mnNumCol[mnn[r][c]], lite);
      tft.drawString(String(mnn[r][c]), x + 12, y + 12, 2);
    }
  }
}

void mnNew() {
  mnMinesV = constrain((diff == 0 ? 7 : diff == 1 ? 10 : diff == 2 ? 13 : 16) + (lvl - 1) / 2, 5, 24);
  memset(mnm, 0, sizeof(mnm)); memset(mnr, 0, sizeof(mnr)); memset(mnn, 0, sizeof(mnn));
  mnrev = 0; mnFirst = true; mnDown = false; mnFired = false;
  tft.fillRect(0, 20, 320, 220, TFT_BLACK);
  for (int r = 0; r < MN_R; r++) for (int c = 0; c < MN_C; c++) mnDraw(r, c);
}

void mnInit() {
  tft.fillScreen(TFT_BLACK);
  mnscore = 0;
  hud("MINAS", 0, -1);
  mnNew();
}

void mnPlace(int r0, int c0) {
  int n = 0;
  while (n < MN_MINES) {
    int r = random(MN_R), c = random(MN_C);
    if (mnm[r][c] || (abs(r - r0) <= 1 && abs(c - c0) <= 1)) continue;
    mnm[r][c] = 1; n++;
  }
  for (int r = 0; r < MN_R; r++) for (int c = 0; c < MN_C; c++) {
    int k = 0;
    for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++) {
      int nr = r + dr, nc = c + dc;
      if ((dr || dc) && nr >= 0 && nr < MN_R && nc >= 0 && nc < MN_C && mnm[nr][nc]) k++;
    }
    mnn[r][c] = k;
  }
}

void mnReveal(int r, int c) {
  static int16_t stk[8 * MN_R * MN_C + 8][2];
  int sp = 0;
  stk[sp][0] = r; stk[sp][1] = c; sp++;
  while (sp) {
    sp--;
    int rr = stk[sp][0], cc = stk[sp][1];
    if (mnr[rr][cc] != 0) continue;
    mnr[rr][cc] = 1; mnrev++; mnscore++;
    mnDraw(rr, cc);
    if (mnn[rr][cc] == 0 && !mnm[rr][cc]) {
      for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++) {
        int nr = rr + dr, nc = cc + dc;
        if ((dr || dc) && nr >= 0 && nr < MN_R && nc >= 0 && nc < MN_C && mnr[nr][nc] == 0 && sp < 8 * MN_R * MN_C) {
          stk[sp][0] = nr; stk[sp][1] = nc; sp++;
        }
      }
    }
  }
}

void mnStep() {
  uint32_t now = millis();
  int cr = -1, cc = -1;
  if (touched && tx >= MN_X && ty >= MN_Y) {
    cc = (tx - MN_X) / MN_S; cr = (ty - MN_Y) / MN_S;
    if (cc >= MN_C || cr >= MN_R) cr = -1;
  }
  if (pressed && cr >= 0) {
    mnDown = true; mnFired = false; mnCr = cr; mnCc = cc; mnT0 = now;
  } else if (mnDown && touched && !mnFired && now - mnT0 > 450) {
    mnFired = true;
    if (mnr[mnCr][mnCc] == 0) mnr[mnCr][mnCc] = 2;
    else if (mnr[mnCr][mnCc] == 2) mnr[mnCr][mnCc] = 0;
    mnDraw(mnCr, mnCc);
    beep(500, 40);
  } else if (mnDown && !touched) {
    mnDown = false;
    if (!mnFired && mnr[mnCr][mnCc] == 0) {
      if (mnFirst) { mnPlace(mnCr, mnCc); mnFirst = false; }
      if (mnm[mnCr][mnCc]) {
        for (int r = 0; r < MN_R; r++) for (int c = 0; c < MN_C; c++) if (mnm[r][c]) { mnr[r][c] = 1; mnDraw(r, c); }
        beep(120, 300);
        delay(1200);
        endGame(18, mnscore);
        return;
      }
      mnReveal(mnCr, mnCc);
      hud("MINAS", mnscore, -1);
      beep(900, 20);
      if (mnrev >= MN_R * MN_C - MN_MINES) {
        mnscore += 50;
        hud("MINAS", mnscore, -1);
        const int f[] = {800, 1000, 1300};
        const int d[] = {100, 100, 200};
        playSeq(f, d, 3);
        mnNew();
      }
    }
  }
}

// ---------- 20) ALVO ----------
#define TG_BG C565(15, 40, 70)
#define TG_R 28
struct Tgt { int x, y; uint32_t t0, dur; bool a; };
Tgt tg[4];
int tgscore, tglives;
uint32_t tgsp, tgfr;

int tgRad(int i, uint32_t now) {
  uint32_t age = now - tg[i].t0;
  if (age >= tg[i].dur) return 0;
  return 4 + (TG_R - 4) * (int)(tg[i].dur - age) / (int)tg[i].dur;
}

void tgInit() {
  tft.fillScreen(TG_BG);
  tgscore = 0; tglives = liv(3);
  for (int i = 0; i < 4; i++) tg[i].a = false;
  tgsp = millis() + 500; tgfr = 0;
  hud("ALVO", 0, tglives);
}

void tgStep() {
  uint32_t now = millis();
  if (pressed) {
    int best = -1, bd = 99999;
    for (int i = 0; i < 4; i++) {
      if (!tg[i].a) continue;
      int r = tgRad(i, now), dx = tx - tg[i].x, dy = ty - tg[i].y, d2 = dx * dx + dy * dy;
      if (d2 <= (r + 8) * (r + 8) && d2 < bd) { bd = d2; best = i; }
    }
    if (best >= 0) {
      int r = tgRad(best, now);
      tft.fillCircle(tg[best].x, tg[best].y, TG_R + 1, TG_BG);
      tg[best].a = false;
      tgscore += (r < 14) ? 3 : (r < 20 ? 2 : 1);
      hud("ALVO", tgscore, tglives);
      beep(1300, 40);
    }
  }
  if (now >= tgsp) {
    for (int i = 0; i < 4; i++) {
      if (!tg[i].a) {
        tg[i].x = random(34, 286); tg[i].y = random(54, 212);
        tg[i].t0 = now; tg[i].dur = max(600, (int)(2300 / SPD())); tg[i].a = true;
        tgsp = now + max(260, (int)(1000 / SPD())) + random(0, 300);
        break;
      }
    }
  }
  if (now - tgfr >= 40) {
    tgfr = now;
    for (int i = 0; i < 4; i++) if (tg[i].a) tft.fillCircle(tg[i].x, tg[i].y, TG_R + 1, TG_BG);
    for (int i = 0; i < 4; i++) {
      if (!tg[i].a) continue;
      if (now - tg[i].t0 >= tg[i].dur) {
        tg[i].a = false;
        tglives--;
        hud("ALVO", tgscore, tglives);
        beep(150, 150);
        if (tglives <= 0) { endGame(19, tgscore); return; }
      }
    }
    for (int i = 0; i < 4; i++) {
      if (!tg[i].a) continue;
      int r = tgRad(i, now);
      tft.fillCircle(tg[i].x, tg[i].y, r, TFT_RED);
      if (r > 8) { tft.fillCircle(tg[i].x, tg[i].y, r * 2 / 3, TFT_WHITE); tft.fillCircle(tg[i].x, tg[i].y, r / 3, TFT_RED); }
    }
  }
}

// ---------- 21) LIGA4 ----------
#define L4X 41
#define L4Y 30
#define L4S 34
uint8_t c4[6][7];
int c4score;

void c4Cell(int r, int c) {
  int cx = L4X + c * L4S + L4S / 2, cy = L4Y + r * L4S + L4S / 2;
  uint16_t col = (c4[r][c] == 1) ? TFT_YELLOW : (c4[r][c] == 2 ? TFT_RED : TFT_BLACK);
  tft.fillCircle(cx, cy, 14, col);
}

void c4Board() {
  tft.fillRect(0, 20, 320, 220, TFT_BLACK);
  tft.fillRoundRect(L4X - 4, L4Y - 4, 7 * L4S + 8, 6 * L4S + 8, 8, C565(30, 60, 210));
  for (int r = 0; r < 6; r++) for (int c = 0; c < 7; c++) c4Cell(r, c);
}

void c4Reset() { memset(c4, 0, sizeof(c4)); c4Board(); }

int c4Drop(int c, uint8_t p) {
  for (int r = 5; r >= 0; r--) if (!c4[r][c]) { c4[r][c] = p; return r; }
  return -1;
}

void c4Undo(int c) {
  for (int r = 0; r < 6; r++) if (c4[r][c]) { c4[r][c] = 0; return; }
}

bool c4Win(uint8_t p) {
  static const int d[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
  for (int r = 0; r < 6; r++) for (int c = 0; c < 7; c++) for (int k = 0; k < 4; k++) {
    int n = 0;
    while (n < 4) {
      int rr = r + d[k][0] * n, cc = c + d[k][1] * n;
      if (rr < 0 || rr > 5 || cc < 0 || cc > 6 || c4[rr][cc] != p) break;
      n++;
    }
    if (n == 4) return true;
  }
  return false;
}

bool c4Full() {
  for (int c = 0; c < 7; c++) if (!c4[0][c]) return false;
  return true;
}

int c4Cpu() {
  int rp = max(0, (diff == 0 ? 55 : diff == 1 ? 28 : diff == 2 ? 10 : 0) - (lvl - 1) * 2);
  if (random(100) < rp) {
    int c, tries = 0;
    do { c = random(7); tries++; } while (c4[0][c] && tries < 30);
    if (!c4[0][c]) return c;
  }
  for (int c = 0; c < 7; c++) {
    if (c4[0][c]) continue;
    c4Drop(c, 2); bool w = c4Win(2); c4Undo(c);
    if (w) return c;
  }
  for (int c = 0; c < 7; c++) {
    if (c4[0][c]) continue;
    c4Drop(c, 1); bool w = c4Win(1); c4Undo(c);
    if (w) return c;
  }
  int best = -1, bs = -999;
  for (int c = 0; c < 7; c++) {
    if (c4[0][c]) continue;
    c4Drop(c, 2);
    int sc = 3 - abs(c - 3) + random(0, 3);
    for (int k = 0; k < 7; k++) {
      if (c4[0][k]) continue;
      c4Drop(k, 1);
      if (c4Win(1)) sc -= 20;
      c4Undo(k);
    }
    c4Undo(c);
    if (sc > bs) { bs = sc; best = c; }
  }
  return best;
}

void c4Init() {
  tft.fillScreen(TFT_BLACK);
  c4score = 0;
  hud("LIGA4", 0, -1);
  c4Reset();
}

void c4Step() {
  if (!pressed || tx < L4X || ty < 22) return;
  int c = (tx - L4X) / L4S;
  if (c > 6) return;
  int r = c4Drop(c, 1);
  if (r < 0) return;
  c4Cell(r, c); beep(900, 40);
  if (c4Win(1)) {
    c4score++;
    hud("LIGA4", c4score, -1);
    const int f[] = {800, 1000, 1300};
    const int d[] = {100, 100, 200};
    playSeq(f, d, 3);
    vBanner("VOCE VENCEU!", TFT_GREEN);
    c4Reset();
    return;
  }
  if (c4Full()) { vBanner("EMPATE", TFT_YELLOW); c4Reset(); return; }
  delay(300);
  int m = c4Cpu();
  int rr = c4Drop(m, 2);
  c4Cell(rr, m); beep(500, 40);
  if (c4Win(2)) { delay(500); endGame(20, c4score); return; }
  if (c4Full()) { vBanner("EMPATE", TFT_YELLOW); c4Reset(); }
}

// ---------- 22) CONTAS (NOVO - matematica) ----------
// Toque na resposta certa antes do tempo acabar. Controle: setas = 4 botoes.
int ctA, ctB, ctOp, ctAns, ctCor, ctScore, ctLives, ctW;
int ctOpt[4];
uint32_t ctT0, ctLim;

void ctBtn(int i, int st) {
  int x = (i & 1) ? 164 : 8, y = (i >> 1) ? 172 : 110;
  uint16_t c = (st == 1) ? C565(30, 160, 60) : (st == 2 ? C565(200, 40, 40) : C565(50, 80, 190));
  tft.fillRoundRect(x, y, 148, 54, 10, c);
  tft.drawRoundRect(x, y, 148, 54, 10, TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, c);
  tft.drawString(String(ctOpt[i]), x + 74, y + 27, 4);
}

void ctNew() {
  int mx = 9 + lvl * 3 + diff * 5;
  int nops = (lvl <= 2 && diff <= 1) ? 2 : ((lvl <= 5 && diff < 3) ? 3 : 4);
  ctOp = random(nops);
  int m2 = min(12, 3 + lvl + diff * 2);
  switch (ctOp) {
    case 0: ctA = random(1, mx + 1); ctB = random(1, mx + 1); ctAns = ctA + ctB; break;
    case 1: ctA = random(2, mx + 1); ctB = random(1, ctA); ctAns = ctA - ctB; break;
    case 2: ctA = random(2, m2 + 1); ctB = random(2, m2 + 1); ctAns = ctA * ctB; break;
    default: ctB = random(2, m2 + 1); ctAns = random(2, m2 + 1); ctA = ctAns * ctB; break;
  }
  ctCor = random(4);
  for (int i = 0; i < 4; i++) {
    if (i == ctCor) { ctOpt[i] = ctAns; continue; }
    int v = 0, tries = 0;
    bool ok;
    do {
      int d = random(1, max(4, ctAns / 4 + 2));
      if (random(2)) d = -d;
      if (random(4) == 0) d = random(2) ? 10 : -10;
      v = ctAns + d;
      ok = (v >= 0 && v != ctAns);
      for (int j = 0; j < i && ok; j++) if (ctOpt[j] == v) ok = false;
      tries++;
    } while (!ok && tries < 40);
    if (!ok) v = ctAns + 20 + i * 3;
    ctOpt[i] = v;
  }
  tft.fillRect(0, 20, 320, 220, TFT_BLACK);
  static const char opc[4] = {'+', '-', 'x', '/'};
  String q = String(ctA) + " " + String(opc[ctOp]) + " " + String(ctB) + " = ?";
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  if (tft.textWidth(q, 4) > 300) tft.setTextSize(1);
  tft.drawString(q, 160, 56, 4);
  tft.setTextSize(1);
  for (int i = 0; i < 4; i++) ctBtn(i, 0);
  static const int base[4] = {10000, 7500, 5800, 4300};
  ctLim = (uint32_t)max(2200, base[diff] - (lvl - 1) * 250);
  ctW = -1;
  ctT0 = millis();
}

void ctInit() {
  tft.fillScreen(TFT_BLACK);
  ctScore = 0; ctLives = liv(3);
  hud("CONTAS", 0, ctLives);
  ctNew();
}

void ctStep() {
  uint32_t now = millis();
  uint32_t el = now - ctT0;
  int w = (el >= ctLim) ? 0 : (int)(300UL * (ctLim - el) / ctLim);
  if (w != ctW) {
    ctW = w;
    uint16_t col = (w > 150) ? TFT_GREEN : (w > 70 ? TFT_YELLOW : TFT_RED);
    tft.fillRect(10, 92, w, 8, col);
    tft.fillRect(10 + w, 92, 300 - w, 8, TFT_DARKGREY);
  }
  int pick = -1;
  if (pressed && ty >= 106) pick = ((ty >= 168) ? 2 : 0) + ((tx >= 160) ? 1 : 0);
  if (pick < 0 && el >= ctLim) pick = 9;
  if (pick < 0) return;
  bool ok = (pick == ctCor);
  if (pick < 4) ctBtn(pick, ok ? 1 : 2);
  ctBtn(ctCor, 1);
  if (ok) {
    ctScore++;
    hud("CONTAS", ctScore, ctLives);
    beep(1300, 60);
    delay(350);
  } else {
    ctLives--;
    hud("CONTAS", ctScore, ctLives);
    beep(150, 250);
    delay(900);
    if (ctLives <= 0) { endGame(21, ctScore); return; }
  }
  ctNew();
}

// ---------- 23) SURF (NOVO - corredor de 3 faixas, estilo Subway Surfers) ----------
// Toque esquerda/direita = troca de faixa | centro de cima = PULA | centro de baixo = ROLA
// Trem vermelho: so desviando de faixa. Barreira amarela: pule. Arco laranja: role por baixo.
#define SF_ROAD C565(45, 48, 62)
#define SF_Y 190
struct SfObj { bool on; int lane, type, h; float y; int oy; };
struct SfCoin { bool on; int lane; float y; int oy; };
SfObj sfo[5];
SfCoin sfc[8];
int sfLane, sfPl, sfScore, sfLives, sfSp, sfLast;
float sfDash;
uint32_t sfJump, sfRoll, sfInv, sfSpawn, sfCoinT;

int sfX(int lane) { return 80 + lane * 80; }

void sfR(int x, int y, int w, int h, uint16_t c) {      // retangulo que nunca invade a barra de cima
  if (y < 20) { h -= 20 - y; y = 20; }
  if (h <= 0 || y >= 240) return;
  if (y + h > 240) h = 240 - y;
  tft.fillRect(x, y, w, h, c);
}

void sfDashes() {
  for (int d = 0; d < 2; d++) {
    int x = 118 + d * 80;
    tft.fillRect(x, 20, 4, 220, SF_ROAD);
    for (int y = (int)sfDash - 30; y < 240; y += 30) sfR(x, y, 4, 16, C565(150, 150, 160));
  }
}

void sfDrawObj(int i) {
  SfObj &o = sfo[i];
  int cx = sfX(o.lane), y = (int)o.y;
  o.oy = y;
  if (o.type == 0) {                       // trem
    sfR(cx - 26, y, 52, o.h, C565(200, 50, 50));
    sfR(cx - 26, y, 52, 6, C565(240, 120, 120));
    sfR(cx - 18, y + 14, 14, 10, C565(180, 230, 255));
    sfR(cx + 4, y + 14, 14, 10, C565(180, 230, 255));
    sfR(cx - 18, y + 38, 14, 10, C565(180, 230, 255));
    sfR(cx + 4, y + 38, 14, 10, C565(180, 230, 255));
  } else if (o.type == 1) {                // barreira baixa
    sfR(cx - 26, y, 52, o.h, TFT_YELLOW);
    for (int k = 0; k < 4; k++) sfR(cx - 22 + k * 14, y, 6, o.h, TFT_BLACK);
  } else {                                 // arco alto (rolar)
    sfR(cx - 28, y, 56, 8, C565(240, 140, 20));
    sfR(cx - 28, y, 6, o.h, C565(240, 140, 20));
    sfR(cx + 22, y, 6, o.h, C565(240, 140, 20));
  }
}

void sfEraseObj(int i) { SfObj &o = sfo[i]; sfR(sfX(o.lane) - 30, o.oy - 1, 60, o.h + 2, SF_ROAD); }

void sfDrawCoin(int i) {
  SfCoin &c = sfc[i];
  int cx = sfX(c.lane), y = (int)c.y;
  c.oy = y;
  if (y < 28 || y > 232) return;
  tft.fillCircle(cx, y, 6, TFT_YELLOW);
  tft.drawCircle(cx, y, 6, C565(200, 140, 0));
  tft.fillRect(cx - 1, y - 3, 2, 6, C565(200, 140, 0));
}

void sfEraseCoin(int i) {
  SfCoin &c = sfc[i];
  if (c.oy < 20 || c.oy > 240) return;
  sfR(sfX(c.lane) - 8, c.oy - 8, 16, 16, SF_ROAD);
}

void sfPlayer(int lane, bool show) {
  int cx = sfX(lane);
  if (!show) { sfR(cx - 18, SF_Y - 36, 36, 64, SF_ROAD); return; }
  uint32_t now = millis();
  bool jmp = now < sfJump, rol = now < sfRoll;
  if (jmp) {
    tft.fillCircle(cx, SF_Y + 16, 8, C565(25, 28, 38));          // sombra
    int y = SF_Y - 18;
    tft.fillCircle(cx, y - 14, 6, C565(255, 210, 160));
    tft.fillRect(cx - 7, y - 20, 14, 5, TFT_RED);
    tft.fillRoundRect(cx - 8, y - 8, 16, 20, 4, C565(40, 120, 240));
    tft.fillRect(cx - 7, y + 12, 5, 8, C565(30, 30, 90));
    tft.fillRect(cx + 2, y + 12, 5, 8, C565(30, 30, 90));
  } else if (rol) {
    tft.fillRoundRect(cx - 14, SF_Y + 2, 28, 14, 7, C565(40, 120, 240));
    tft.fillCircle(cx + 12, SF_Y + 8, 5, C565(255, 210, 160));
    tft.fillRect(cx + 8, SF_Y + 2, 8, 3, TFT_RED);
  } else {
    int y = SF_Y;
    tft.fillCircle(cx, y - 14, 6, C565(255, 210, 160));
    tft.fillRect(cx - 7, y - 20, 14, 5, TFT_RED);
    tft.fillRoundRect(cx - 8, y - 8, 16, 20, 4, C565(40, 120, 240));
    tft.fillRect(cx - 7, y + 12, 5, 8, C565(30, 30, 90));
    tft.fillRect(cx + 2, y + 12, 5, 8, C565(30, 30, 90));
  }
}

void surfInit() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 20, 320, 220, C565(30, 110, 50));
  tft.fillRect(40, 20, 240, 220, SF_ROAD);
  tft.fillRect(36, 20, 4, 220, TFT_LIGHTGREY);
  tft.fillRect(280, 20, 4, 220, TFT_LIGHTGREY);
  sfLane = sfPl = 1; sfLast = 1;
  sfScore = 0; sfLives = liv(3);
  sfSp = constrain((int)(3.5f * SPD() + 0.5f), 2, 11);
  sfDash = 0; sfJump = sfRoll = sfInv = 0;
  for (int i = 0; i < 5; i++) sfo[i].on = false;
  for (int i = 0; i < 8; i++) sfc[i].on = false;
  sfSpawn = millis() + 900;
  sfCoinT = millis() + 400;
  hud("SURF", 0, sfLives);
  sfDashes();
  sfPlayer(sfLane, true);
}

void sfAddObj(int lane, int type) {
  for (int i = 0; i < 5; i++) if (!sfo[i].on) {
    sfo[i].on = true; sfo[i].lane = lane; sfo[i].type = type;
    sfo[i].h = (type == 0) ? 70 : (type == 1 ? 14 : 22);
    sfo[i].y = -sfo[i].h; sfo[i].oy = -200;
    return;
  }
}

void surfStep() {
  uint32_t now = millis();

  // controle
  if (pressed) {
    if (tx < 107) sfLane = max(0, sfLane - 1);
    else if (tx > 213) sfLane = min(2, sfLane + 1);
    else if (ty < 130) { sfJump = now + 520; sfRoll = 0; beep(900, 40); }
    else { sfRoll = now + 520; sfJump = 0; beep(500, 40); }
  }

  // apaga tudo que vai mexer
  sfPlayer(sfPl, false);
  for (int i = 0; i < 5; i++) if (sfo[i].on) sfEraseObj(i);
  for (int i = 0; i < 8; i++) if (sfc[i].on) sfEraseCoin(i);

  // aparece obstaculo
  if (now >= sfSpawn) {
    sfSpawn = now + (uint32_t)((int)(1500 / SPD()) + random(0, 350));
    int lane = random(3), r = random(100);
    int type = (r < 45) ? 0 : (r < 75 ? 1 : 2);
    sfAddObj(lane, type);
    sfLast = lane;
    if (random(100) < 30) {
      int l2 = (lane + 1 + random(2)) % 3;
      sfAddObj(l2, random(1, 3));
    }
  }
  // aparecem moedas
  if (now >= sfCoinT) {
    sfCoinT = now + (uint32_t)((int)(1100 / SPD()) + random(0, 300));
    int lane = (sfLast + 1 + random(2)) % 3;
    for (int k = 0; k < 4; k++)
      for (int i = 0; i < 8; i++) if (!sfc[i].on) {
        sfc[i].on = true; sfc[i].lane = lane; sfc[i].y = -10 - k * 28; sfc[i].oy = -200;
        break;
      }
  }

  bool jmp = now < sfJump, rol = now < sfRoll;
  bool dead = false;
  for (int i = 0; i < 5; i++) {
    if (!sfo[i].on) continue;
    SfObj &o = sfo[i];
    o.y += sfSp;
    if (o.y > 245) { o.on = false; continue; }
    if (o.lane == sfLane && o.y < SF_Y + 16 && o.y + o.h > SF_Y - 16 && now > sfInv) {
      bool hit = (o.type == 0) || (o.type == 1 && !jmp) || (o.type == 2 && !rol);
      if (hit) {
        o.on = false;
        sfLives--;
        sfInv = now + 1500;
        hud("SURF", sfScore, sfLives);
        beep(150, 250);
        if (sfLives <= 0) dead = true;
        continue;
      }
    }
    if (o.y > SF_Y + 20 && o.h > 0 && o.lane >= 0 && !(o.type > 9)) {
      // passou por ele: conta ponto uma vez (marca com type+10)
      o.type += 10;
      sfScore++;
      hud("SURF", sfScore, sfLives);
    }
  }
  for (int i = 0; i < 8; i++) {
    if (!sfc[i].on) continue;
    SfCoin &c = sfc[i];
    c.y += sfSp;
    if (c.y > 245) { c.on = false; continue; }
    if (c.lane == sfLane && fabsf(c.y - SF_Y) < 20) {
      c.on = false;
      sfScore++;
      hud("SURF", sfScore, sfLives);
      beep(1500, 25);
    }
  }
  if (dead) { endGame(22, sfScore); return; }

  // desenha
  sfDash += sfSp;
  if (sfDash >= 30) sfDash -= 30;
  sfDashes();
  for (int i = 0; i < 8; i++) if (sfc[i].on) sfDrawCoin(i);
  for (int i = 0; i < 5; i++) if (sfo[i].on) {
    int t = sfo[i].type;
    if (t >= 10) sfo[i].type = t - 10;
    sfDrawObj(i);
    if (t >= 10) sfo[i].type = t;
  }
  sfPl = sfLane;
  if (now > sfInv || ((now / 100) & 1)) sfPlayer(sfPl, true);
}

// ---------- 24) INVASORES (NOVO - estilo Space Invaders) ----------
// Arraste o dedo para mover a nave (ela atira sozinha). Nao deixe os invasores chegarem embaixo.
#define IV_R 4
#define IV_C 8
bool ivA[IV_R][IV_C];
int ivX, ivY, ivDir, ivAlive, ivScore, ivLives, ivPx, ivBx, ivBy, ivWave;
bool ivB;
struct IvBomb { bool on; int x; float y; };
IvBomb ivBm[3];
uint32_t ivMove, ivFire, ivDrop, ivInv;

void ivDrawAlien(int r, int c, bool show) {
  int x = ivX + c * 32 + 5, y = ivY + r * 22 + 4;
  if (!show) { tft.fillRect(x - 2, y - 1, 26, 18, TFT_BLACK); return; }
  uint16_t col = (r == 0) ? C565(230, 60, 200) : (r == 1 ? C565(60, 200, 240) : (r == 2 ? C565(80, 220, 90) : C565(250, 200, 50)));
  tft.fillRoundRect(x + 2, y, 18, 10, 4, col);
  tft.fillRect(x, y + 3, 22, 4, col);
  tft.fillRect(x + 5, y + 3, 3, 3, TFT_BLACK);
  tft.fillRect(x + 14, y + 3, 3, 3, TFT_BLACK);
  tft.fillRect(x + 3, y + 10, 3, 4, col);
  tft.fillRect(x + 16, y + 10, 3, 4, col);
}

void ivDrawAll() {
  for (int r = 0; r < IV_R; r++)
    for (int c = 0; c < IV_C; c++) if (ivA[r][c]) ivDrawAlien(r, c, true);
}

void ivShip(bool show) {
  if (!show) { tft.fillRect(ivPx - 15, 212, 30, 20, TFT_BLACK); return; }
  tft.fillTriangle(ivPx, 214, ivPx - 13, 228, ivPx + 13, 228, C565(60, 230, 90));
  tft.fillRect(ivPx - 13, 226, 26, 4, C565(30, 160, 60));
}

void ivNewWave() {
  for (int r = 0; r < IV_R; r++) for (int c = 0; c < IV_C; c++) ivA[r][c] = true;
  ivAlive = IV_R * IV_C;
  ivX = 30; ivY = 28; ivDir = 1;
  for (int i = 0; i < 3; i++) ivBm[i].on = false;
  tft.fillRect(0, 20, 320, 190, TFT_BLACK);
  ivDrawAll();
  ivMove = millis() + 500;
}

void invInit() {
  tft.fillScreen(TFT_BLACK);
  ivScore = 0; ivLives = liv(3); ivWave = 0;
  ivPx = 160; ivB = false; ivInv = 0;
  ivFire = millis() + 400; ivDrop = millis() + 1200;
  hud("INVASORES", 0, ivLives);
  ivNewWave();
  ivShip(true);
}

void invStep() {
  uint32_t now = millis();

  // nave
  int old = ivPx;
  if (touched) ivPx += constrain(tx - ivPx, -9, 9);
  ivPx = constrain(ivPx, 16, 304);
  if (ivPx != old) { int o2 = ivPx; ivPx = old; ivShip(false); ivPx = o2; }
  if (!ivB && now >= ivFire) { ivB = true; ivBx = ivPx; ivBy = 208; ivFire = now + 280; beep(1200, 15); }

  // tiro da nave
  if (ivB) {
    tft.fillRect(ivBx - 1, ivBy, 3, 9, TFT_BLACK);
    ivBy -= 9;
    if (ivBy < 22) ivB = false;
    else {
      int c = (ivBx - ivX) / 32, r = (ivBy - ivY) / 22;
      if (ivBx >= ivX && c >= 0 && c < IV_C && r >= 0 && r < IV_R && ivBy >= ivY && ivA[r][c]) {
        int ax = ivX + c * 32 + 3, ay = ivY + r * 22 + 3;
        if (ivBx >= ax && ivBx < ax + 24 && ivBy >= ay && ivBy < ay + 18) {
          ivA[r][c] = false; ivAlive--; ivB = false;
          ivDrawAlien(r, c, false);
          ivScore++;
          hud("INVASORES", ivScore, ivLives);
          beep(900, 30);
        }
      }
      if (ivB) tft.fillRect(ivBx - 1, ivBy, 3, 9, TFT_WHITE);
    }
  }
  if (ivAlive == 0) {              // onda limpa
    ivWave++;
    const int f[] = {800, 1000, 1300};
    const int d[] = {80, 80, 160};
    playSeq(f, d, 3);
    ivB = false;
    ivNewWave();
    ivShip(true);
    return;
  }

  // formacao anda
  if (now >= ivMove) {
    ivMove = now + (uint32_t)max(30, (int)((30 + ivAlive * 9) / (SPD() * (1.0f + 0.08f * ivWave))));
    int minC = IV_C, maxC = -1, low = 0;
    for (int r = 0; r < IV_R; r++) for (int c = 0; c < IV_C; c++) if (ivA[r][c]) {
      if (c < minC) minC = c;
      if (c > maxC) maxC = c;
      if (r > low) low = r;
    }
    tft.fillRect(ivX, ivY, IV_C * 32, IV_R * 22, TFT_BLACK);
    int left = ivX + minC * 32 + 5, right = ivX + maxC * 32 + 27;
    if ((ivDir > 0 && right + 8 > 316) || (ivDir < 0 && left - 8 < 4)) { ivDir = -ivDir; ivY += 10; }
    else ivX += ivDir * 8;
    ivDrawAll();
    if (ivY + low * 22 + 18 >= 205) { endGame(23, ivScore); return; }
  }

  // bombas
  if (now >= ivDrop) {
    ivDrop = now + (uint32_t)(max(450, (int)(1300 / SPD())) + random(0, 300));
    int c = random(IV_C);
    for (int t = 0; t < IV_C; t++) {
      int cc = (c + t) % IV_C, r = -1;
      for (int rr = IV_R - 1; rr >= 0; rr--) if (ivA[rr][cc]) { r = rr; break; }
      if (r < 0) continue;
      for (int i = 0; i < 3; i++) if (!ivBm[i].on) {
        ivBm[i].on = true; ivBm[i].x = ivX + cc * 32 + 16; ivBm[i].y = ivY + r * 22 + 18;
        break;
      }
      break;
    }
  }
  int bs = constrain((int)(3.0f * SPD() + 0.5f), 2, 8);
  for (int i = 0; i < 3; i++) {
    if (!ivBm[i].on) continue;
    tft.fillRect(ivBm[i].x - 2, (int)ivBm[i].y, 5, 10, TFT_BLACK);
    ivBm[i].y += bs;
    if (ivBm[i].y > 236) { ivBm[i].on = false; continue; }
    if (abs(ivBm[i].x - ivPx) < 14 && ivBm[i].y + 10 > 214 && ivBm[i].y < 230 && now > ivInv) {
      ivBm[i].on = false;
      ivLives--;
      ivInv = now + 1300;
      hud("INVASORES", ivScore, ivLives);
      beep(150, 250);
      if (ivLives <= 0) { endGame(23, ivScore); return; }
      continue;
    }
    tft.fillRect(ivBm[i].x - 1, (int)ivBm[i].y, 3, 10, TFT_RED);
    tft.fillRect(ivBm[i].x - 2, (int)ivBm[i].y + 6, 5, 3, C565(255, 140, 0));
  }
  if (now > ivInv || ((now / 100) & 1)) ivShip(true);
}

// =====================================================
//        NIVEIS E METAS DE PONTOS (todos os jogos)
//   Cada jogo tem uma meta. Ao bater a meta: "NIVEL COMPLETO",
//   o jogo fica mais dificil e a proxima meta e maior.
//   Passou do nivel 10 = VOCE ZEROU!
// =====================================================
int lvScoreNow() {
  switch (mode) {
    case G_FLAPPY:  return fscore;
    case G_COBRA:   return sscore;
    case G_BLOCOS:  return bscore;
    case G_TOCA:    return mscore;
    case G_GENIUS:  return glen - 1;
    case G_CORRIDA: return rscore;
    case G_PONG:    return pscore;
    case G_VELHA:   return vscore;
    case G_NAVE:    return nscore;
    case G_TORRE:   return tk - 1;
    case G_2048:    return g2score;
    case G_TETRIS:  return tscore;
    case G_LABIR:   return lbscore;
    case G_FRUTAS:  return frsc;
    case G_CARROS:  return csc;
    case G_MEMORIA: return mmScore;
    case G_REFLEXO: return rfScore;
    case G_SALTO:   return sjscore;
    case G_MINAS:   return mnscore;
    case G_ALVO:    return tgscore;
    case G_LIGA4:   return c4score;
    case G_CONTAS:  return ctScore;
    case G_SURF:    return sfScore;
    case G_INVAS:   return ivScore;
  }
  return 0;
}

int *lvScorePtr() {            // jogos que reiniciam o campo a cada nivel mantendo os pontos
  switch (mode) {
    case G_FLAPPY:  return &fscore;
    case G_COBRA:   return &sscore;
    case G_BLOCOS:  return &bscore;
    case G_TOCA:    return &mscore;
    case G_CORRIDA: return &rscore;
    case G_PONG:    return &pscore;
    case G_VELHA:   return &vscore;
    case G_NAVE:    return &nscore;
    case G_LABIR:   return &lbscore;
    case G_FRUTAS:  return &frsc;
    case G_CARROS:  return &csc;
    case G_MEMORIA: return &mmScore;
    case G_REFLEXO: return &rfScore;
    case G_SALTO:   return &sjscore;
    case G_MINAS:   return &mnscore;
    case G_ALVO:    return &tgscore;
    case G_LIGA4:   return &c4score;
    case G_CONTAS:  return &ctScore;
    case G_SURF:    return &sfScore;
    case G_INVAS:   return &ivScore;
  }
  return nullptr;
}

int *lvLivesPtr() {
  switch (mode) {
    case G_BLOCOS:  return &blives;
    case G_TOCA:    return &mlives;
    case G_PONG:    return &plives;
    case G_NAVE:    return &nlives;
    case G_FRUTAS:  return &frlv;
    case G_MEMORIA: return &mmLives;
    case G_REFLEXO: return &rfLives;
    case G_ALVO:    return &tglives;
    case G_CONTAS:  return &ctLives;
    case G_SURF:    return &sfLives;
    case G_INVAS:   return &ivLives;
  }
  return nullptr;
}

void gameInit(int g) {
  switch (g) {
    case 0:  flappyInit(); break;
    case 1:  cobraInit();  break;
    case 2:  blocosInit(); break;
    case 3:  tocaInit();   break;
    case 4:  geniusInit(); break;
    case 5:  corridaInit(); break;
    case 6:  pongInit();   break;
    case 7:  velhaInit();  break;
    case 8:  naveInit();   break;
    case 9:  torreInit();  break;
    case 10: g2Init();     break;
    case 11: tetrisInit(); break;
    case 12: labInit();    break;
    case 13: frInit();     break;
    case 14: carInit();    break;
    case 15: mmInit();     break;
    case 16: rfInit();     break;
    case 17: sjInit();     break;
    case 18: mnInit();     break;
    case 19: tgInit();     break;
    case 20: c4Init();     break;
    case 21: ctInit();     break;
    case 22: surfInit();   break;
    case 23: invInit();    break;
  }
}

void lvResume(int g) {         // volta ao jogo depois do "NIVEL COMPLETO"
  if (g == 4) {                // GENIUS: continua a sequencia
    tft.fillScreen(TFT_BLACK);
    for (int p = 0; p < 4; p++) gPanel(p, false);
    hud("GENIUS", glen - 1, -1);
    gplay = true; gidx = 0;
    return;
  }
  if (g == 9) {                // TORRE: a torre continua
    tft.fillScreen(TFT_BLACK);
    for (int i = tscroll; i < tk; i++) tDraw(i);
    hud("TORRE", tk - 1, -1);
    return;
  }
  if (g == 10) {               // 2048: o tabuleiro continua
    tft.fillScreen(TFT_BLACK);
    tft.fillRoundRect(G2X, G2Y, 212, 212, 6, C565(120, 110, 100));
    g2Draw();
    hud("2048", g2score, -1);
    return;
  }
  if (g == 11) {               // TETRIS: o tabuleiro continua
    tft.fillScreen(TFT_BLACK);
    hud("TETRIS", tscore, -1);
    tft.drawRect(TX0 - 2, 19, TC * 10 + 3, 222, TFT_WHITE);
    tSide();
    tDrawAll();
    tfallT = millis() + 500;
    return;
  }
  int *sp = lvScorePtr(), *lp = lvLivesPtr();
  int s = sp ? *sp : 0, l = lp ? *lp : -1;
  gameInit(g);                 // campo novo, pontos e vidas mantidos
  if (sp) *sp = s;
  if (lp) *lp = l;
  if (g == 17) sjbase = s;
  hud(defNames[g], s, l);
}

void lvUp(int g, int s) {
  musicStop();
  if (lvl >= LVMAX) { wonAll = true; endGame(g, s); return; }
  do { lvl++; lvGoal += lvStep(g, lvl); } while (lvGoal <= s && lvl < LVMAX);
  const int f[] = {600, 800, 1000, 1300};
  const int d[] = {100, 100, 100, 250};
  playSeq(f, d, 4);
  tft.fillScreen(BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(themeLight ? C565(0, 140, 40) : TFT_GREEN, BG);
  tft.drawString("NIVEL " + String(lvl - 1) + " COMPLETO!", 160, 46, 4);
  tft.setTextColor(ACC, BG);
  tft.drawString("Proximo: NIVEL " + String(lvl), 160, 96, 4);
  tft.setTextColor(FG, BG);
  tft.drawString("Meta: " + String(lvGoal) + " pontos", 160, 138, 2);
  tft.drawString("Dificuldade: " + String(DNAME[diff]), 160, 160, 2);
  tft.setTextColor(SUBC, BG);
  tft.drawString("Toque para continuar", 160, 206, 2);
  waitTap();
  lvResume(g);
  musicPlay(g + 1);
}

void lvAfterStep() {           // chamado depois de cada quadro do jogo
  if (mode == MENU) return;
  int s = lvScoreNow();
  if (s >= lvGoal) lvUp(mode - 1, s);
}

// ---------- Gerador de QR Code embutido (versao 3, ECC L, ate 53 bytes) ----------
// Nao precisa de biblioteca externa.
#define QN 29
static uint8_t qm[QN][QN], qf[QN][QN];

static void qSet(int x, int y, bool d) {
  if (x < 0 || y < 0 || x >= QN || y >= QN) return;
  qm[y][x] = d; qf[y][x] = 1;
}

static uint8_t qMul(uint8_t a, uint8_t b) {
  uint8_t z = 0;
  for (int i = 7; i >= 0; i--) {
    z = (uint8_t)((z << 1) ^ ((z >> 7) * 0x11D));
    z ^= ((b >> i) & 1) * a;
  }
  return z;
}

static void qFinder(int cx, int cy) {
  for (int dy = -4; dy <= 4; dy++)
    for (int dx = -4; dx <= 4; dx++) {
      int d = max(abs(dx), abs(dy));
      qSet(cx + dx, cy + dy, d != 2 && d != 4);
    }
}

static void qFormat(int mask) {
  int data = (1 << 3) | mask;               // ECC L = 01
  int rem = data;
  for (int i = 0; i < 10; i++) rem = (rem << 1) ^ ((rem >> 9) * 0x537);
  int bits = ((data << 10) | rem) ^ 0x5412;
  for (int i = 0; i <= 5; i++) qSet(8, i, (bits >> i) & 1);
  qSet(8, 7, (bits >> 6) & 1);
  qSet(8, 8, (bits >> 7) & 1);
  qSet(7, 8, (bits >> 8) & 1);
  for (int i = 9; i < 15; i++) qSet(14 - i, 8, (bits >> i) & 1);
  for (int i = 0; i < 8; i++) qSet(QN - 1 - i, 8, (bits >> i) & 1);
  for (int i = 8; i < 15; i++) qSet(8, QN - 15 + i, (bits >> i) & 1);
  qSet(8, QN - 8, true);                    // modulo escuro fixo
}

// Gera o QR de "txt" (ate 53 caracteres) em qm[][]. Retorna false se for grande demais.
static bool qBuild(const char *txt) {
  int len = strlen(txt);
  if (len > 53) return false;
  memset(qm, 0, sizeof(qm)); memset(qf, 0, sizeof(qf));

  // ---- dados: modo byte ----
  uint8_t data[70];
  memset(data, 0, sizeof(data));
  int nb = 0;
  auto put = [&](uint32_t v, int n) {
    for (int i = n - 1; i >= 0; i--) {
      if ((v >> i) & 1) data[nb >> 3] |= 0x80 >> (nb & 7);
      nb++;
    }
  };
  put(4, 4);
  put(len, 8);
  for (int i = 0; i < len; i++) put((uint8_t)txt[i], 8);
  put(0, min(4, 440 - nb));
  while (nb & 7) nb++;
  for (uint8_t pad = 0xEC; nb < 440; pad ^= (0xEC ^ 0x11)) put(pad, 8);

  // ---- Reed-Solomon (15 codigos de correcao) ----
  uint8_t gen[15] = {0};
  gen[14] = 1;
  uint8_t root = 1;
  for (int i = 0; i < 15; i++) {
    for (int j = 0; j < 15; j++) {
      gen[j] = qMul(gen[j], root);
      if (j + 1 < 15) gen[j] ^= gen[j + 1];
    }
    root = qMul(root, 0x02);
  }
  uint8_t rem[15] = {0};
  for (int i = 0; i < 55; i++) {
    uint8_t f = data[i] ^ rem[0];
    memmove(rem, rem + 1, 14);
    rem[14] = 0;
    for (int j = 0; j < 15; j++) rem[j] ^= qMul(gen[j], f);
  }
  memcpy(data + 55, rem, 15);

  // ---- padroes fixos ----
  for (int i = 0; i < QN; i++) { qSet(6, i, i % 2 == 0); qSet(i, 6, i % 2 == 0); }
  qFinder(3, 3); qFinder(QN - 4, 3); qFinder(3, QN - 4);
  for (int dy = -2; dy <= 2; dy++)          // alinhamento em (22,22)
    for (int dx = -2; dx <= 2; dx++)
      qSet(22 + dx, 22 + dy, max(abs(dx), abs(dy)) != 1);
  qFormat(0);

  // ---- posiciona os bits ----
  int bi = 0;
  for (int right = QN - 1; right >= 1; right -= 2) {
    if (right == 6) right = 5;
    for (int vert = 0; vert < QN; vert++) {
      for (int j = 0; j < 2; j++) {
        int x = right - j;
        bool up = ((right + 1) & 2) == 0;
        int y = up ? QN - 1 - vert : vert;
        if (!qf[y][x] && bi < 560) {
          qm[y][x] = (data[bi >> 3] >> (7 - (bi & 7))) & 1;
          bi++;
        }
      }
    }
  }

  // ---- mascara 0 e informacao de formato ----
  for (int y = 0; y < QN; y++)
    for (int x = 0; x < QN; x++)
      if (!qf[y][x] && ((x + y) % 2 == 0)) qm[y][x] ^= 1;
  qFormat(0);
  return true;
}

// =====================================================
//                    MENU / ICONES / QR
// =====================================================
const uint16_t bcol[NGAMES] = {
  C565(30, 120, 230), C565(20, 150, 60), C565(60, 60, 200),
  C565(0, 150, 150), C565(160, 40, 200), C565(220, 110, 10),
  C565(90, 90, 110), C565(210, 40, 90), C565(30, 30, 130), C565(170, 150, 10),
  C565(230, 150, 30), C565(20, 110, 210), C565(110, 70, 170), C565(30, 170, 80), C565(190, 70, 40),
  C565(200, 60, 140), C565(30, 170, 70), C565(60, 150, 230), C565(100, 110, 160), C565(220, 50, 50), C565(40, 70, 200),
  C565(0, 140, 120), C565(0, 120, 170), C565(110, 50, 170)
};
#define BW 98
#define BH 62
const int bxs[PAGE_SZ] = {8, 111, 214, 8, 111, 214};
const int bys[PAGE_SZ] = {34, 34, 34, 102, 102, 102};

void drawIcon(int g, int cx, int cy) {
  switch (g) {
    case 0:  // flappy
      tft.fillRect(cx + 6, cy - 13, 10, 12, F_PIPE);
      tft.fillRect(cx + 6, cy + 5, 10, 8, F_PIPE);
      tft.fillRect(cx - 14, cy - 6, 12, 12, TFT_YELLOW);
      tft.fillRect(cx - 6, cy - 3, 3, 3, TFT_BLACK);
      tft.fillRect(cx - 2, cy + 1, 4, 3, TFT_ORANGE);
      break;
    case 1:  // cobra
      tft.fillRect(cx - 14, cy + 4, 7, 7, TFT_GREEN);
      tft.fillRect(cx - 7, cy + 4, 7, 7, TFT_GREEN);
      tft.fillRect(cx, cy + 4, 7, 7, TFT_GREEN);
      tft.fillRect(cx, cy - 3, 7, 7, TFT_GREEN);
      tft.fillRect(cx, cy - 10, 7, 7, TFT_GREENYELLOW);
      tft.fillRect(cx + 11, cy - 10, 6, 6, TFT_RED);
      break;
    case 2:  // blocos
      tft.fillRect(cx - 14, cy - 12, 8, 5, TFT_RED);
      tft.fillRect(cx - 4, cy - 12, 8, 5, TFT_YELLOW);
      tft.fillRect(cx + 6, cy - 12, 8, 5, TFT_CYAN);
      tft.fillCircle(cx + 2, cy + 1, 2, TFT_WHITE);
      tft.fillRect(cx - 8, cy + 10, 16, 3, TFT_WHITE);
      break;
    case 3:  // toca
      tft.fillCircle(cx, cy - 1, 9, M_BROWN);
      tft.fillCircle(cx - 3, cy - 4, 2, TFT_WHITE);
      tft.fillCircle(cx + 3, cy - 4, 2, TFT_WHITE);
      tft.fillCircle(cx, cy + 1, 2, TFT_PINK);
      tft.fillRect(cx - 16, cy + 6, 32, 4, M_GRASS);
      tft.fillEllipse(cx, cy + 11, 14, 4, M_HOLE);
      break;
    case 4:  // genius
      tft.fillRect(cx - 12, cy - 11, 11, 10, TFT_RED);
      tft.fillRect(cx + 1,  cy - 11, 11, 10, TFT_GREEN);
      tft.fillRect(cx - 12, cy + 1,  11, 10, TFT_BLUE);
      tft.fillRect(cx + 1,  cy + 1,  11, 10, TFT_YELLOW);
      break;
    case 5:  // corrida
      tft.fillRect(cx - 14, cy - 6, 9, 10, TFT_WHITE);
      tft.fillRect(cx - 12, cy + 4, 3, 5, TFT_WHITE);
      tft.fillRect(cx - 8,  cy - 4, 2, 2, TFT_BLACK);
      tft.fillRect(cx + 6, cy - 9, 5, 18, TFT_GREEN);
      tft.fillRect(cx + 2, cy - 3, 4, 3, TFT_GREEN);
      tft.fillRect(cx + 11, cy - 6, 3, 3, TFT_GREEN);
      tft.fillRect(cx - 16, cy + 10, 32, 1, TFT_WHITE);
      break;
    case 6:  // pong
      tft.fillRect(cx - 14, cy - 9, 3, 14, TFT_WHITE);
      tft.fillRect(cx + 11, cy - 3, 3, 14, TFT_RED);
      tft.fillRect(cx - 2, cy - 2, 5, 5, TFT_WHITE);
      break;
    case 7:  // velha
      tft.drawFastVLine(cx - 4, cy - 12, 24, TFT_WHITE);
      tft.drawFastVLine(cx + 4, cy - 12, 24, TFT_WHITE);
      tft.drawFastHLine(cx - 12, cy - 4, 24, TFT_WHITE);
      tft.drawFastHLine(cx - 12, cy + 4, 24, TFT_WHITE);
      tft.drawLine(cx - 11, cy - 11, cx - 6, cy - 6, TFT_CYAN);
      tft.drawLine(cx - 6, cy - 11, cx - 11, cy - 6, TFT_CYAN);
      tft.drawCircle(cx, cy, 3, TFT_RED);
      tft.drawLine(cx + 5, cy + 5, cx + 10, cy + 10, TFT_CYAN);
      tft.drawLine(cx + 10, cy + 5, cx + 5, cy + 10, TFT_CYAN);
      break;
    case 8:  // nave
      tft.fillTriangle(cx - 6, cy - 4, cx - 13, cy + 11, cx + 1, cy + 11, TFT_CYAN);
      tft.fillRect(cx + 4, cy - 12, 11, 7, TFT_RED);
      tft.fillRect(cx - 7, cy - 12, 2, 5, TFT_YELLOW);
      break;
    case 9:  // torre
      tft.fillRect(cx - 14, cy + 7, 28, 5, TFT_MAGENTA);
      tft.fillRect(cx - 10, cy + 1, 22, 5, TFT_ORANGE);
      tft.fillRect(cx - 6, cy - 5, 14, 5, TFT_YELLOW);
      tft.fillRect(cx - 3, cy - 11, 8, 5, TFT_GREEN);
      break;
    case 10: // 2048
      tft.fillRect(cx - 12, cy - 11, 11, 10, C565(238, 228, 218));
      tft.fillRect(cx + 1,  cy - 11, 11, 10, C565(242, 177, 121));
      tft.fillRect(cx - 12, cy + 1,  11, 10, C565(246, 124, 95));
      tft.fillRect(cx + 1,  cy + 1,  11, 10, C565(237, 194, 46));
      break;
    case 11: // tetris
      tft.fillRect(cx - 14, cy - 12, 6, 6, TFT_CYAN);
      tft.fillRect(cx - 8,  cy - 12, 6, 6, TFT_CYAN);
      tft.fillRect(cx - 2,  cy - 12, 6, 6, TFT_CYAN);
      tft.fillRect(cx - 8,  cy - 6,  6, 6, TFT_CYAN);
      tft.fillRect(cx + 4,  cy + 2,  6, 6, TFT_ORANGE);
      tft.fillRect(cx + 10, cy + 2,  6, 6, TFT_ORANGE);
      tft.fillRect(cx + 4,  cy + 8,  6, 6, TFT_ORANGE);
      tft.fillRect(cx + 10, cy + 8,  6, 6, TFT_ORANGE);
      break;
    case 12: // labirinto
      tft.fillRect(cx - 14, cy - 12, 28, 2, TFT_WHITE);
      tft.fillRect(cx - 14, cy + 10, 28, 2, TFT_WHITE);
      tft.fillRect(cx - 14, cy - 12, 2, 24, TFT_WHITE);
      tft.fillRect(cx + 12, cy - 12, 2, 24, TFT_WHITE);
      tft.fillRect(cx - 6, cy - 12, 2, 14, TFT_WHITE);
      tft.fillRect(cx + 2, cy - 2, 2, 12, TFT_WHITE);
      tft.fillRect(cx - 6, cy - 2, 10, 2, TFT_WHITE);
      tft.fillCircle(cx - 9, cy - 6, 2, TFT_YELLOW);
      tft.fillRect(cx + 6, cy + 4, 4, 4, TFT_GREEN);
      break;
    case 13: // frutas
      tft.fillCircle(cx - 6, cy - 4, 6, TFT_RED);
      tft.fillRect(cx - 7, cy - 12, 2, 4, TFT_GREEN);
      tft.fillCircle(cx + 7, cy + 1, 5, TFT_ORANGE);
      tft.fillRect(cx - 13, cy + 8, 26, 6, C565(160, 110, 50));
      break;
    case 14: // carros
      tft.fillRect(cx - 12, cy - 13, 24, 26, C565(60, 60, 60));
      tft.fillRect(cx - 9, cy - 11, 8, 10, TFT_YELLOW);
      tft.fillRect(cx - 1, cy - 11, 2, 0, TFT_WHITE);
      tft.fillRect(cx + 1, cy + 2, 8, 10, TFT_RED);
      tft.fillRect(cx - 1, cy - 12, 2, 24, TFT_DARKGREY);
      break;
    case 15: // memoria
      tft.fillRoundRect(cx - 15, cy - 11, 14, 20, 3, TFT_WHITE);
      tft.fillCircle(cx - 8, cy - 1, 4, TFT_RED);
      tft.fillRoundRect(cx + 1, cy - 11, 14, 20, 3, C565(60, 60, 200));
      tft.drawRoundRect(cx + 1, cy - 11, 14, 20, 3, TFT_WHITE);
      break;
    case 16: // reflexo
      tft.fillCircle(cx, cy, 13, TFT_GREEN);
      tft.fillRect(cx - 2, cy - 8, 4, 10, TFT_WHITE);
      tft.fillRect(cx - 2, cy + 5, 4, 4, TFT_WHITE);
      break;
    case 17: // salto
      tft.fillRect(cx - 14, cy + 9, 28, 4, TFT_GREEN);
      tft.fillRect(cx - 5, cy - 4, 10, 10, TFT_YELLOW);
      tft.fillRect(cx - 2, cy - 1, 2, 2, TFT_BLACK);
      tft.drawFastVLine(cx, cy - 11, 5, TFT_WHITE);
      break;
    case 18: // minas
      tft.fillRect(cx - 13, cy - 11, 12, 10, C565(90, 110, 200));
      tft.fillRect(cx + 1, cy - 11, 12, 10, TFT_LIGHTGREY);
      tft.fillRect(cx - 13, cy + 1, 12, 10, TFT_LIGHTGREY);
      tft.fillRect(cx + 1, cy + 1, 12, 10, C565(90, 110, 200));
      tft.fillCircle(cx + 7, cy - 6, 3, TFT_BLACK);
      tft.fillTriangle(cx - 10, cy + 3, cx - 10, cy + 9, cx - 4, cy + 6, TFT_RED);
      break;
    case 19: // alvo
      tft.fillCircle(cx, cy, 13, TFT_RED);
      tft.fillCircle(cx, cy, 9, TFT_WHITE);
      tft.fillCircle(cx, cy, 5, TFT_RED);
      tft.fillCircle(cx, cy, 2, TFT_WHITE);
      break;
    case 20: // liga4
      tft.fillRoundRect(cx - 14, cy - 12, 28, 24, 4, C565(30, 60, 200));
      tft.fillCircle(cx - 7, cy - 5, 4, TFT_YELLOW);
      tft.fillCircle(cx + 7, cy - 5, 4, TFT_RED);
      tft.fillCircle(cx - 7, cy + 6, 4, TFT_RED);
      tft.fillCircle(cx + 7, cy + 6, 4, TFT_YELLOW);
      break;
    case 21: // contas
      tft.fillRect(cx - 14, cy - 2, 11, 3, TFT_WHITE);
      tft.fillRect(cx - 10, cy - 6, 3, 11, TFT_WHITE);
      tft.fillRect(cx + 1, cy - 5, 11, 3, TFT_WHITE);
      tft.fillRect(cx + 1, cy + 1, 11, 3, TFT_WHITE);
      tft.fillCircle(cx - 8, cy + 10, 2, TFT_YELLOW);
      tft.fillCircle(cx + 6, cy + 10, 2, TFT_YELLOW);
      break;
    case 22: // surf
      tft.drawLine(cx - 6, cy - 14, cx - 14, cy + 14, TFT_WHITE);
      tft.drawLine(cx + 6, cy - 14, cx + 14, cy + 14, TFT_WHITE);
      tft.fillRect(cx - 4, cy - 4, 8, 10, TFT_RED);
      tft.fillCircle(cx, cy - 8, 3, TFT_YELLOW);
      tft.fillCircle(cx + 9, cy + 8, 2, TFT_YELLOW);
      break;
    case 23: // invasores
      tft.fillRoundRect(cx - 9, cy - 8, 18, 10, 4, TFT_GREEN);
      tft.fillRect(cx - 12, cy - 5, 24, 4, TFT_GREEN);
      tft.fillRect(cx - 5, cy - 5, 3, 3, TFT_BLACK);
      tft.fillRect(cx + 3, cy - 5, 3, 3, TFT_BLACK);
      tft.fillRect(cx - 8, cy + 2, 3, 4, TFT_GREEN);
      tft.fillRect(cx + 6, cy + 2, 3, 4, TFT_GREEN);
      tft.fillTriangle(cx, cy + 6, cx - 6, cy + 14, cx + 6, cy + 14, TFT_WHITE);
      break;
  }
}

uint16_t MB() { return (menuBgSet && !bgOn) ? menuBg : BG; }   // cor de fundo do menu

void drawVol() {
  tft.fillRect(118, 176, 84, 56, MB());
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(SUBC, MB());
  tft.drawString("VOLUME", 160, 184, 2);
  for (int i = 0; i < 5; i++) {
    int h = 6 + i * 5;
    tft.fillRect(128 + i * 13, 222 - h, 9, h, (i < vol) ? (themeLight ? C565(0, 160, 60) : TFT_GREEN) : TFT_DARKGREY);
  }
}

void drawClock() {
  int h, m;
  char b[8];
  if (getClock(h, m)) snprintf(b, sizeof(b), "%02d:%02d", h, m);
  else strcpy(b, "--:--");
  tft.fillRoundRect(262, 3, 56, 26, 6, MB());
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(ACC, MB());
  tft.drawString(b, 290, 16, 2);
}

int mcur = 0;                 // item selecionado no menu (controle/teclado)
bool showCur = false;         // so mostra o destaque quando usa setas
bool lastWifiOk = false;

void drawQrBtn() {
  bool on = (WiFi.status() == WL_CONNECTED);
  uint16_t c = on ? C565(60, 220, 100) : TFT_WHITE;
  tft.fillRoundRect(4, 3, 46, 26, 6, c);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_BLACK, c);
  tft.drawString("QR", 27, 16, 2);
  lastWifiOk = on;
}

void drawBatIcon() {
  if (!batBeta || !batHas) return;
  tft.fillRect(4, 163, 126, 16, MB());
  uint16_t col = (batPct > 50) ? C565(60, 200, 90) : (batPct > 20 ? TFT_YELLOW : TFT_RED);
  tft.drawRect(8, 165, 26, 12, FG);
  tft.fillRect(34, 168, 3, 6, FG);
  int w = constrain(batPct, 0, 100) * 22 / 100;
  if (w > 0) tft.fillRect(10, 167, w, 8, col);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(FG, MB());
  tft.drawString(String(batPct) + "%", 40, 171, 1);
  if (batChg) { tft.setTextColor(TFT_YELLOW, MB()); tft.drawString("carregando", 70, 171, 1); }
  tft.setTextDatum(MC_DATUM);
}

void drawBgImage() {
  File f = LittleFS.open("/bg.raw", "r");
  if (!f || f.size() != 153600) { if (f) f.close(); tft.fillScreen(BG); return; }
  static uint16_t row[320];
  bool sw = tft.getSwapBytes();
  tft.setSwapBytes(true);                 // corrige a ordem dos bytes (era isso que deixava as cores estranhas)
  for (int y = 0; y < 240; y++) {
    f.read((uint8_t *)row, 640);
    tft.pushImage(0, y, 320, 1, row);
  }
  tft.setSwapBytes(sw);
  f.close();
}

void menuDraw() {
  if (curTrack != 0) musicPlay(0);
  if (bgOn) drawBgImage(); else tft.fillScreen(MB());
  tft.setTextDatum(MC_DATUM);

  // faixa do titulo
  tft.fillRoundRect(86, 3, 172, 26, 6, MB());
  tft.setTextColor(ACC2, MB());
  tft.drawString("PIXEL ARCADE", 172, 16, 4);

  // botoes QR (verde = WiFi conectado) e configuracoes
  drawQrBtn();
  tft.fillRoundRect(53, 3, 32, 26, 6, C565(90, 90, 130));
  tft.setTextColor(TFT_WHITE, C565(90, 90, 130));
  tft.drawString("CFG", 69, 16, 1);
  drawClock();

  for (int s = 0; s < PAGE_SZ; s++) {
    int i = mpage * PAGE_SZ + s;
    if (i >= NGAMES) break;
    tft.fillRoundRect(bxs[s], bys[s], BW, BH, 10, bcol[i]);
    tft.drawRoundRect(bxs[s], bys[s], BW, BH, 10, TFT_WHITE);
    if (showCur && s == mcur) {
      for (int k = 1; k <= 3; k++) tft.drawRoundRect(bxs[s] - k, bys[s] - k, BW + 2 * k, BH + 2 * k, 10, TFT_YELLOW);
    }
    drawIcon(i, bxs[s] + BW / 2, bys[s] + 17);
    tft.setTextColor(TFT_WHITE, bcol[i]);
    tft.drawString(gname[i], bxs[s] + BW / 2, bys[s] + 40, 2);
    tft.setTextColor(TFT_LIGHTGREY, bcol[i]);
    tft.drawString("Rec: " + String(hs[i]), bxs[s] + BW / 2, bys[s] + 54, 1);
  }

  // bolinhas de pagina
  for (int p = 0; p < NPAGES; p++)
    tft.fillCircle(160 + (p * 2 - (NPAGES - 1)) * 7, 171, 3, p == mpage ? FG : TFT_DARKGREY);

  // botoes de pagina e volume
  tft.fillRoundRect(8, 180, 44, 48, 10, TFT_DARKGREY);
  tft.fillRoundRect(268, 180, 44, 48, 10, TFT_DARKGREY);
  tft.fillRoundRect(60, 180, 56, 48, 10, TFT_DARKGREY);
  tft.fillRoundRect(204, 180, 56, 48, 10, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawString("<", 30, 204, 4);
  tft.drawString(">", 290, 204, 4);
  tft.drawString("-", 88, 204, 4);
  tft.drawString("+", 232, 204, 4);
  drawVol();
  drawBatIcon();
}

// ---------- Teclado na tela (para digitar nome/senha do WiFi) ----------
const char *KB_L[3][4] = {
  {"1234567890", "qwertyuiop", "asdfghjkl@", "zxcvbnm._-"},
  {"1234567890", "QWERTYUIOP", "ASDFGHJKL@", "ZXCVBNM._-"},
  {"1234567890", "!@#$%^&*()", "-_=+[]{};:", "'\",.<>/?|~"}
};

void kbKeys(int layer) {
  tft.fillRect(0, 52, 320, 188, BG);
  tft.setTextDatum(MC_DATUM);
  uint16_t kc = C565(60, 60, 130);
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 10; c++) {
      int x = c * 32 + 1, y = 56 + r * 36;
      tft.fillRoundRect(x, y, 30, 32, 5, kc);
      tft.setTextColor(TFT_WHITE, kc);
      char s[2] = {KB_L[layer][r][c], 0};
      tft.drawString(s, x + 15, y + 17, 2);
    }
  uint16_t sc = C565(90, 90, 150);
  tft.fillRoundRect(2, 202, 56, 32, 5, layer == 1 ? ACC : sc);
  tft.fillRoundRect(62, 202, 56, 32, 5, layer == 2 ? ACC : sc);
  tft.fillRoundRect(122, 202, 100, 32, 5, kc);
  tft.fillRoundRect(226, 202, 44, 32, 5, C565(200, 110, 30));
  tft.fillRoundRect(274, 202, 44, 32, 5, C565(30, 160, 60));
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Aa", 30, 218, 2);
  tft.drawString("#+=", 90, 218, 2);
  tft.drawString("ESPACO", 172, 218, 2);
  tft.drawString("<-", 248, 218, 2);
  tft.drawString("OK", 296, 218, 2);
}

void kbField(const String &txt, const char *title) {
  tft.fillRect(0, 0, 320, 52, BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(ACC2, BG);
  tft.drawString(title, 6, 3, 2);
  tft.drawRoundRect(4, 22, 280, 26, 5, FG);
  String shown = txt.length() > 28 ? txt.substring(txt.length() - 28) : txt;
  tft.setTextColor(FG, BG);
  tft.drawString(shown + "_", 8, 27, 2);
  tft.fillRoundRect(288, 22, 28, 26, 5, TFT_RED);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.drawString("X", 302, 35, 2);
}

bool kbEdit(const char *title, String &val) {      // true = OK, false = cancelou
  String t = val;
  int layer = 0;
  tft.fillScreen(BG);
  kbField(t, title);
  kbKeys(layer);
  do { netPoll(); readTouch(); delay(10); } while (touched);
  while (true) {
    netPoll(); readTouch(); delay(10);
    if (padE & K_MENU) return false;
    if (!pressed) continue;
    bool keys = false;
    if (ty >= 20 && ty < 50 && tx >= 286) return false;
    if (ty >= 56 && ty < 200) {
      int r = constrain((ty - 56) / 36, 0, 3), c = constrain(tx / 32, 0, 9);
      if (t.length() < 63) t += KB_L[layer][r][c];
      beep(1000, 15);
    } else if (ty >= 202) {
      if (tx < 60) { layer = (layer == 1) ? 0 : 1; keys = true; }
      else if (tx < 120) { layer = (layer == 2) ? 0 : 2; keys = true; }
      else if (tx < 224) { if (t.length() < 63) t += ' '; }
      else if (tx < 272) { if (t.length()) t.remove(t.length() - 1); }
      else { val = t; beep(1200, 40); return true; }
      beep(800, 15);
    } else continue;
    kbField(t, title);
    if (keys) kbKeys(layer);
  }
}

// ---------- WiFi: procurar redes, escolher e conectar ----------
String scSsid[14];
int scRssi[14], scN = 0;
bool scOpen[14];

void wifiScan() {
  tft.fillScreen(BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(ACC2, BG);
  tft.drawString("Procurando redes...", 160, 110, 4);
  tft.setTextColor(SUBC, BG);
  tft.drawString("(so aparecem redes 2.4 GHz)", 160, 145, 2);
  wifiBusy = true;
  if (WiFi.status() != WL_CONNECTED) { WiFi.disconnect(false); delay(150); }
  int n = WiFi.scanNetworks(false, false);
  scN = 0;
  for (int i = 0; i < n && scN < 14; i++) {
    String s = WiFi.SSID(i);
    if (!s.length()) continue;
    bool dup = false;
    for (int j = 0; j < scN; j++)
      if (scSsid[j] == s) { dup = true; if (WiFi.RSSI(i) > scRssi[j]) scRssi[j] = WiFi.RSSI(i); break; }
    if (dup) continue;
    scSsid[scN] = s;
    scRssi[scN] = WiFi.RSSI(i);
    scOpen[scN] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
    scN++;
  }
  WiFi.scanDelete();
  for (int i = 0; i < scN; i++)                       // mais forte primeiro
    for (int j = i + 1; j < scN; j++)
      if (scRssi[j] > scRssi[i]) {
        String ts = scSsid[i]; scSsid[i] = scSsid[j]; scSsid[j] = ts;
        int tr = scRssi[i]; scRssi[i] = scRssi[j]; scRssi[j] = tr;
        bool to = scOpen[i]; scOpen[i] = scOpen[j]; scOpen[j] = to;
      }
  wifiBusy = false;
  wifiTry = millis();
}

void wifiConnectUI(const String &ss, const String &pw) {
  wifiBusy = true;
  wifiSsid = ss; wifiPass = pw;
  prefs.putString("ws", wifiSsid);
  prefs.putString("wp", wifiPass);
  WiFi.disconnect(false);
  delay(150);
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  tft.fillScreen(BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(ACC2, BG);
  tft.drawString("Conectando...", 160, 80, 4);
  tft.setTextColor(FG, BG);
  tft.drawString(ss.length() > 28 ? ss.substring(0, 28) : ss, 160, 120, 2);
  uint32_t t0 = millis();
  int dots = 0;
  while (millis() - t0 < 20000 && WiFi.status() != WL_CONNECTED) {
    netPoll();
    delay(250);
    tft.fillRect(100, 150, 120, 16, BG);
    tft.setTextColor(SUBC, BG);
    String d = "";
    for (int i = 0; i <= (dots % 4); i++) d += ".";
    tft.drawString(d, 160, 158, 2);
    dots++;
  }
  wifiBusy = false;
  wifiTry = millis();
  staUp = false;
  bool ok = (WiFi.status() == WL_CONNECTED);
  tft.fillScreen(BG);
  tft.setTextDatum(MC_DATUM);
  if (ok) {
    beep(1200, 80);
    tft.setTextColor(themeLight ? C565(0, 140, 40) : TFT_GREEN, BG);
    tft.drawString("CONECTADO!", 160, 50, 4);
    tft.setTextColor(FG, BG);
    tft.drawString("Endereco do controle:", 160, 100, 2);
    tft.setTextColor(ACC, BG);
    tft.drawString("http://" + WiFi.localIP().toString() + "/esp32", 160, 128, 2);
    tft.setTextColor(SUBC, BG);
    tft.drawString("Toque em QR no menu para abrir", 160, 160, 2);
  } else {
    beep(200, 250);
    tft.setTextColor(TFT_RED, BG);
    tft.drawString("NAO CONECTOU", 160, 40, 4);
    tft.setTextColor(FG, BG);
    tft.drawString("Confira a senha e se o hotspot", 160, 90, 2);
    tft.drawString("esta ligado em 2.4 GHz.", 160, 110, 2);
    tft.setTextColor(SUBC, BG);
    tft.drawString("O ESP32 nao usa WiFi 5 GHz.", 160, 134, 2);
    tft.drawString("Ele vai tentar de novo sozinho.", 160, 154, 2);
  }
  tft.setTextColor(SUBC, BG);
  tft.drawString("Toque para continuar", 160, 215, 2);
  waitTap();
}

void wifiList(int page) {
  tft.fillScreen(BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(ACC2, BG);
  tft.drawString("ESCOLHA O WIFI (2.4GHz)", 6, 8, 2);
  tft.fillRoundRect(238, 3, 78, 26, 6, C565(90, 90, 150));
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, C565(90, 90, 150));
  tft.drawString("MANUAL", 277, 16, 2);
  for (int i = 0; i < 5; i++) {
    int idx = page * 5 + i;
    if (idx >= scN) break;
    int y = 34 + i * 30;
    tft.fillRoundRect(6, y, 308, 27, 6, C565(40, 40, 90));
    String s = scSsid[idx];
    if (s.length() > 24) s = s.substring(0, 23) + "~";
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(TFT_WHITE, C565(40, 40, 90));
    tft.drawString(s, 14, y + 14, 2);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(TFT_YELLOW, C565(40, 40, 90));
    tft.drawString(String(scRssi[idx]) + (scOpen[idx] ? "" : " *"), 308, y + 14, 2);
  }
  if (!scN) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(SUBC, BG);
    tft.drawString("Nenhuma rede encontrada", 160, 100, 2);
    tft.drawString("Ligue o hotspot e toque BUSCAR", 160, 124, 2);
  }
  uint16_t bc = TFT_DARKGREY;
  tft.fillRoundRect(6, 196, 50, 40, 8, bc);
  tft.fillRoundRect(62, 196, 90, 40, 8, bc);
  tft.fillRoundRect(158, 196, 90, 40, 8, C565(200, 50, 50));
  tft.fillRoundRect(264, 196, 50, 40, 8, bc);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("<", 31, 216, 4);
  tft.drawString("BUSCAR", 107, 216, 2);
  tft.drawString("VOLTAR", 203, 216, 2);
  tft.drawString(">", 289, 216, 4);
}

void wifiScreen() {
  wifiScan();
  int page = 0;
  wifiList(page);
  do { netPoll(); readTouch(); delay(10); } while (touched);
  while (true) {
    netPoll(); readTouch(); delay(10);
    if (padE & (K_MENU | K_B)) return;
    if (!pressed) continue;
    if (ty >= 196) {
      if (tx < 56) { if (page > 0) page--; wifiList(page); }
      else if (tx >= 264) { if ((page + 1) * 5 < scN) page++; wifiList(page); }
      else if (tx >= 62 && tx < 152) { wifiScan(); page = 0; wifiList(page); }
      else if (tx >= 158 && tx < 248) return;
      continue;
    }
    if (ty < 32 && tx >= 238) {                      // digitar o nome manualmente
      String ss = "", pw = "";
      if (kbEdit("Nome do WiFi", ss) && ss.length()) {
        kbEdit("Senha (vazio = rede aberta)", pw);
        wifiConnectUI(ss, pw);
        return;
      }
      wifiList(page);
      continue;
    }
    if (ty >= 34 && ty < 34 + 5 * 30) {
      int idx = page * 5 + (ty - 34) / 30;
      if (idx >= scN) continue;
      String ss = scSsid[idx], pw = "";
      if (!scOpen[idx]) {
        if (ss == wifiSsid) pw = wifiPass;
        String ttl = "Senha de " + ss;
        if (!kbEdit(ttl.c_str(), pw)) { wifiList(page); continue; }
      }
      wifiConnectUI(ss, pw);
      return;
    }
  }
}

// ---------- Ajuste manual da hora (plano B se a internet nao der a hora) ----------
void clockDraw(int h, int m) {
  tft.fillRect(0, 30, 320, 70, BG);
  char b[8];
  snprintf(b, sizeof(b), "%02d:%02d", h, m);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(ACC, BG);
  tft.setTextSize(2);
  tft.drawString(b, 160, 66, 4);
  tft.setTextSize(1);
}

void clockScreen() {
  int h = 12, m = 0;
  getClock(h, m);
  tft.fillScreen(BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(ACC2, BG);
  tft.drawString("AJUSTAR HORA (Rondonia)", 160, 14, 2);
  clockDraw(h, m);
  const char *lb[4] = {"H -", "H +", "M -", "M +"};
  for (int i = 0; i < 4; i++) {
    tft.fillRoundRect(8 + i * 78, 108, 70, 44, 8, C565(60, 60, 130));
    tft.setTextColor(TFT_WHITE, C565(60, 60, 130));
    tft.drawString(lb[i], 43 + i * 78, 130, 4);
  }
  tft.fillRoundRect(8, 172, 148, 50, 10, C565(30, 160, 60));
  tft.fillRoundRect(164, 172, 148, 50, 10, C565(200, 50, 50));
  tft.setTextColor(TFT_WHITE, C565(30, 160, 60));
  tft.drawString("SALVAR", 82, 197, 4);
  tft.setTextColor(TFT_WHITE, C565(200, 50, 50));
  tft.drawString("VOLTAR", 238, 197, 4);
  tft.setTextColor(SUBC, BG);
  tft.drawString("Segure o botao para correr", 160, 160, 1);
  do { netPoll(); readTouch(); delay(10); } while (touched);
  uint32_t rep = 0;
  while (true) {
    netPoll(); readTouch(); delay(10);
    if (padE & (K_MENU | K_B)) return;
    uint32_t now = millis();
    if (pressed) rep = now + 450;
    if (!touched) continue;
    if (ty >= 172) {
      if (!pressed) continue;
      if (tx < 160) { setClockManual(h, m); beep(1200, 60); }
      return;
    }
    if (ty >= 108 && ty < 152 && (pressed || now >= rep)) {
      int i = constrain((tx - 8) / 78, 0, 3);
      if (i == 0) h = (h + 23) % 24;
      else if (i == 1) h = (h + 1) % 24;
      else if (i == 2) m = (m + 59) % 60;
      else m = (m + 1) % 60;
      rep = now + (pressed ? 450 : 110);
      beep(900, 10);
      clockDraw(h, m);
    }
  }
}

// ---------- Tela do QR / endereco do controle ----------
void drawQR(const char *txt, int x, int y) {
  const int m = 4, pad = 8;
  tft.fillRect(x, y, QN * m + pad * 2, QN * m + pad * 2, TFT_WHITE);
  if (!qBuild(txt)) return;
  for (int j = 0; j < QN; j++)
    for (int i = 0; i < QN; i++)
      if (qm[j][i]) tft.fillRect(x + pad + i * m, y + pad + j * m, m, m, TFT_BLACK);
}

void qrScreen() {
  bool on = (WiFi.status() == WL_CONNECTED);
  tft.fillScreen(BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(ACC2, BG);
  tft.drawString("JOGUE NO CELULAR OU NO PC", 160, 12, 2);
  if (on) {
    String ip = WiFi.localIP().toString();
    String url = "http://" + ip + "/esp32";
    drawQR(url.c_str(), 8, 28);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(FG, BG);
    tft.drawString("1) Celular/PC no", 150, 34, 2);
    tft.drawString("mesmo WiFi do ESP", 150, 52, 2);
    tft.drawString("2) Abra o endereco:", 150, 80, 2);
    tft.setTextColor(ACC, BG);
    tft.drawString(ip, 150, 98, 2);
    tft.drawString("/esp32", 150, 116, 2);
    tft.setTextColor(SUBC, BG);
    tft.drawString("ou pixelarcade.local/esp32", 150, 140, 1);
    tft.drawString("PC: setas/WASD + Espaco", 150, 154, 1);
    tft.setTextDatum(MC_DATUM);
    String w = "WiFi: " + wifiSsid;
    if (w.length() > 34) w = w.substring(0, 34);
    tft.drawString(w, 160, 176, 2);
    tft.setTextColor(TFT_DARKGREY, BG);
    tft.drawString("Toque na tela para voltar", 160, 230, 2);
  } else {
    tft.setTextColor(TFT_RED, BG);
    tft.drawString("SEM WIFI", 160, 50, 4);
    tft.setTextColor(FG, BG);
    tft.drawString("1) Ligue o hotspot do celular (2.4 GHz)", 160, 88, 2);
    tft.drawString("2) Toque no botao abaixo e escolha a rede", 160, 110, 2);
    tft.fillRoundRect(50, 140, 220, 44, 10, C565(60, 60, 200));
    tft.setTextColor(TFT_WHITE, C565(60, 60, 200));
    tft.drawString("CONFIGURAR WIFI", 160, 162, 4);
    tft.setTextColor(TFT_DARKGREY, BG);
    tft.drawString("Toque fora do botao para voltar", 160, 226, 2);
  }

  do { netPoll(); readTouch(); delay(10); } while (touched);
  int lastC = -1;
  do {
    netPoll();
    readTouch();
    if (on) {
      int c = (lastInput && millis() - lastInput < 2500) ? 1 : 0;
      if (c != lastC) {
        lastC = c;
        tft.fillRect(0, 196, 320, 22, BG);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(c ? (themeLight ? C565(0, 140, 40) : TFT_GREEN) : SUBC, BG);
        tft.drawString(c ? "Controle conectado!" : "Aguardando o controle...", 160, 206, 2);
      }
    } else if (pressed && tx >= 50 && tx < 270 && ty >= 140 && ty < 184) {
      beep(1000, 40);
      wifiScreen();
      mode = MENU;
      menuDraw();
      return;
    }
    delay(10);
  } while (!anyConfirm() && !(padE & K_MENU));
  mode = MENU;
  menuDraw();
}

// ---------- Configuracoes ----------
void recordsScreen() {
  tft.fillScreen(BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(ACC2, BG);
  tft.drawString("RECORDES (toque p/ voltar)", 160, 9, 2);
  for (int i = 0; i < NGAMES; i++) {
    int col = i / 12, row = i % 12;
    int x = col ? 166 : 6, y = 22 + row * 17;
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(FG, BG);
    tft.drawString(gname[i], x, y, 2);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(ACC, BG);
    tft.drawString(String(hs[i]), x + 148, y, 2);
  }
  waitTap();
}

void cfgBtn(int x, int y, String text, bool on) {
  uint16_t c = C565(60, 60, 130);
  tft.fillRoundRect(x, y, 150, 32, 8, c);
  tft.drawRoundRect(x, y, 150, 32, 8, on ? TFT_GREEN : TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, c);
  tft.drawString(text, x + 75, y + 16, 2);
}

void cfgDraw() {
  tft.fillScreen(BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(ACC2, BG);
  tft.drawString("CONFIGURACOES", 160, 12, 4);
  const char *gm[3] = {"NORMAL", "VIVO", "SUAVE"};
  bool on = (WiFi.status() == WL_CONNECTED);
  int h, m;
  bool hok = getClock(h, m);
  String hv2 = hok ? (String(h < 10 ? "0" : "") + String(h) + ":" + String(m < 10 ? "0" : "") + String(m)) : String("--:--");
  cfgBtn(8, 26, String("TEMA: ") + (themeLight ? "CLARO" : "ESCURO"), true);
  cfgBtn(162, 26, String("MUSICA: ") + (musicOn ? "SIM" : "NAO"), musicOn);
  cfgBtn(8, 62, String("CORES: ") + gm[gammaMode], gammaMode == 1);
  cfgBtn(162, 62, String("INVERTER: ") + (invertOn ? "SIM" : "NAO"), false);
  cfgBtn(8, 98, String("BRILHO: ") + String(bright) + "/5", true);
  cfgBtn(162, 98, "RECORDES", true);
  cfgBtn(8, 134, on ? "WIFI: CONECTADO" : "WIFI: ESCOLHER", on);
  cfgBtn(162, 134, "HORA: " + hv2, hok);

  tft.setTextColor(FG, BG);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("V" VERSION " novidades", 8, 182, 2);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(batHas ? (batChg ? TFT_GREEN : ACC) : SUBC, BG);
  tft.drawString(batText(), 312, 182, 2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(SUBC, BG);
  String ip = on ? ("IP " + WiFi.localIP().toString() + "/esp32") : String("WiFi: nao conectado");
  tft.drawString(ip, 160, 199, 2);

  tft.fillRoundRect(8, 210, 304, 28, 8, C565(200, 50, 50));
  tft.setTextColor(TFT_WHITE, C565(200, 50, 50));
  tft.drawString("VOLTAR", 160, 224, 2);
}

void updateBanner();

void settingsScreen() {
  cfgDraw();
  do { netPoll(); readTouch(); delay(10); } while (touched);
  while (true) {
    netPoll(); readTouch(); delay(10);
    if (padE & (K_MENU | K_B)) break;
    if (!pressed) continue;
    if (ty >= 210) break;
    bool ch = true;
    bool left = (tx < 160);
    if (ty >= 26 && ty < 58) {
      if (left) { themeLight ^= 1; prefs.putInt("thm", themeLight); setTheme(); }
      else { musicOn ^= 1; prefs.putInt("mus", musicOn); if (!musicOn) toneDuty(0, 0); }
    } else if (ty >= 62 && ty < 94) {
      if (left) { gammaMode = (gammaMode + 1) % 3; prefs.putInt("gam", gammaMode); }
      else { invertOn ^= 1; prefs.putInt("inv", invertOn); }
      applyDisplay();
    } else if (ty >= 98 && ty < 130) {
      if (left) { bright = bright % 5 + 1; prefs.putInt("brt", bright); setBright(bright); }
      else { beep(900, 40); recordsScreen(); }
    } else if (ty >= 134 && ty < 166) {
      beep(900, 40);
      if (left) wifiScreen(); else clockScreen();
    } else if (ty >= 172 && ty < 192) {
      beep(900, 40);
      if (tx < 160) updateBanner();
      else { batBeta = !batBeta; prefs.putBool("bat", batBeta); batT = 0; batUpdate(true); }
    } else ch = false;
    if (ch) { beep(900, 40); cfgDraw(); do { netPoll(); readTouch(); delay(10); } while (touched); }
  }
  mode = MENU;
  menuDraw();
}

const char *GT[NGAMES][2] = {
  {"Toque na tela para voar", "Passe pelos canos"},
  {"Toque esquerda/direita da tela", "para virar a cobra"},
  {"Arraste o dedo para mover", "a raquete e quebre os blocos"},
  {"Toque nas toupeiras", "antes que elas fujam"},
  {"Veja a sequencia de cores e sons", "e repita tocando nos quadrados"},
  {"Toque na tela para pular", "os cactos"},
  {"Arraste o dedo para cima e para baixo", "e nao deixe a bola passar"},
  {"Voce e o X. Toque na casa", "para jogar contra o computador"},
  {"Arraste o dedo para mover a nave", "ela atira sozinha"},
  {"Toque para soltar o bloco", "e empilhe o mais alto possivel"},
  {"Deslize o dedo para mover", "e junte numeros iguais"},
  {"Toque esq/dir para mover, centro girar", "segure o centro embaixo para descer"},
  {"Toque na direcao em que quer ir", "e chegue ao quadrado verde"},
  {"Arraste para mover a cesta", "pegue as frutas e fuja das bombas"},
  {"Toque esq/dir para trocar de faixa", "e desvie dos outros carros"},
  {"Toque nas cartas e ache", "os pares iguais"},
  {"Toque quando a tela ficar verde", "nao toque antes da hora"},
  {"Arraste o dedo para mover", "e pule de plataforma em plataforma"},
  {"Toque abre, segure poe bandeira", "nao pise nas minas"},
  {"Toque nos alvos", "antes que eles sumam"},
  {"Voce e o amarelo. Toque na coluna", "e junte 4 em linha"},
  {"Toque na resposta certa", "antes do tempo acabar"},
  {"Toque esq/dir: troca de faixa. Meio: cima pula,", "baixo rola. Desvie dos trens e pegue moedas"},
  {"Arraste o dedo para mover a nave", "ela atira sozinha. Destrua os invasores"}
};

void startGame(int g) {
  mode = g + 1;
  vpx = 160; vpy = 120;
  musicPlay(g + 1);
  startScreen(defNames[g], GT[g][0], GT[g][1]);
  if (!pickDiff()) { mode = MENU; menuDraw(); return; }
  lvl = 1;
  lvGoal = lvStep(g, 1);
  wonAll = false;
  gameInit(g);
}

void menuStep() {
  if (batUpdate(false)) drawBatIcon();
  // relogio: atualiza quando o minuto muda
  static int lastStamp = -2;
  int h, mi;
  int stamp = getClock(h, mi) ? h * 60 + mi : -1;
  if (stamp != lastStamp) { lastStamp = stamp; drawClock(); }
  if ((WiFi.status() == WL_CONNECTED) != lastWifiOk) drawQrBtn();

  // controle / teclado do computador: setas movem o destaque, A abre o jogo
  if (padE & (K_LEFT | K_RIGHT | K_UP | K_DOWN)) {
    showCur = true;
    int cnt = min(PAGE_SZ, NGAMES - mpage * PAGE_SZ);
    if (mcur >= cnt) mcur = cnt - 1;
    if (padE & K_RIGHT) {
      mcur++;
      if (mcur >= cnt) { mpage = (mpage + 1) % NPAGES; mcur = 0; }
    } else if (padE & K_LEFT) {
      mcur--;
      if (mcur < 0) { mpage = (mpage + NPAGES - 1) % NPAGES; mcur = min(PAGE_SZ, NGAMES - mpage * PAGE_SZ) - 1; }
    } else if (padE & K_DOWN) {
      if (mcur + 3 < cnt) mcur += 3;
    } else if (padE & K_UP) {
      if (mcur >= 3) mcur -= 3;
    }
    beep(800, 20);
    menuDraw();
    return;
  }
  if (padE & K_A) {
    int i = mpage * PAGE_SZ + mcur;
    if (i < NGAMES) { beep(1000, 50); startGame(i); return; }
  }

  if (!pressed) return;
  showCur = false;
  if (ty < 32 && tx < 52) { beep(1000, 50); qrScreen(); return; }
  if (ty < 32 && tx >= 53 && tx < 86) { beep(1000, 50); settingsScreen(); return; }
  for (int s = 0; s < PAGE_SZ; s++) {
    int i = mpage * PAGE_SZ + s;
    if (i >= NGAMES) break;
    if (tx >= bxs[s] && tx < bxs[s] + BW && ty >= bys[s] && ty < bys[s] + BH) {
      beep(1000, 50);
      startGame(i);
      return;
    }
  }
  if (ty >= 178 && ty < 230) {
    if (tx >= 8 && tx < 52) {
      mpage = (mpage + NPAGES - 1) % NPAGES; mcur = 0; beep(800, 40); menuDraw();
    } else if (tx >= 268 && tx < 312) {
      mpage = (mpage + 1) % NPAGES; mcur = 0; beep(800, 40); menuDraw();
    } else if (tx >= 60 && tx < 116 && vol > 0) {
      vol--; prefs.putInt("vol", vol); drawVol(); beep(880, 120);
    } else if (tx >= 204 && tx < 260 && vol < 5) {
      vol++; prefs.putInt("vol", vol); drawVol(); beep(880, 120);
    }
  }
}

// =====================================================
//                    ABERTURA (SPLASH)
// =====================================================
void drawTitle(int shift, int count) {
  tft.setTextSize(2);
  tft.setTextDatum(TL_DATUM);
  const char *L[2] = {"PIXEL", "ARCADE"};
  int yy[2] = {26, 82};
  int n = 0;
  for (int l = 0; l < 2; l++) {
    int len = strlen(L[l]);
    int x = (320 - tft.textWidth(L[l], 4)) / 2;
    for (int i = 0; i < len; i++, n++) {
      if (n >= count) { tft.setTextSize(1); return; }
      char s[2] = {L[l][i], 0};
      tft.setTextColor(wheel((uint8_t)(shift + n * 22)), TFT_BLACK);
      tft.drawString(s, x, yy[l], 4);
      x += tft.textWidth(s, 4);
    }
  }
  tft.setTextSize(1);
}

void updateBanner() {          // SUPER ATUALIZACAO NO PIXEL ARCADE V9.1
  musicStop();
  tft.fillScreen(TFT_BLACK);
  const int f[] = {523, 659, 784, 1047, 784, 1047, 1319};
  const int d[] = {110, 110, 110, 160, 90, 90, 300};
  int fi = 0, frame = 0;
  uint32_t t0 = millis(), nextN = 0, endT = t0 + 4200;
  while (millis() < endT) {
    netPoll();
    readTouch();
    frame++;
    if (fi < 7 && millis() >= nextN) { beep(f[fi], d[fi]); nextN = millis() + d[fi] + 20; fi++; }
    for (int k = 0; k < 8; k++) tft.drawPixel(random(4, 316), random(4, 236), wheel((uint8_t)random(256)));
    tft.drawRoundRect(2, 2, 316, 236, 12, wheel((uint8_t)(frame * 8)));
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(wheel((uint8_t)(frame * 10)), TFT_BLACK);
    tft.drawString("SUPER ATUALIZACAO", 160, 40, 4);
    tft.setTextColor(wheel((uint8_t)(frame * 10 + 90)), TFT_BLACK);
    tft.drawString("NO PIXEL ARCADE", 160, 78, 4);
    tft.setTextSize(3);
    tft.setTextColor(wheel((uint8_t)(frame * 10 + 170)), TFT_BLACK);
    tft.drawString("V9.1", 160, 160, 4);
    tft.setTextSize(1);
    delay(35);
    if (millis() > t0 + 900 && anyConfirm()) break;
  }
  sndTone(0); sndEnd = 0;

  // lista de novidades
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("NOVIDADES DA V9.1", 160, 16, 4);
  const char *nv[6] = {"Imagem de fundo sem cor estranha", "Cor de fundo do menu corrigida", "Bateria e carregando (beta)",
                       "Reiniciar o ESP pelo site", "Erro do SURF corrigido", "24 jogos | 4 dificuldades"};
  tft.setTextDatum(ML_DATUM);
  for (int i = 0; i < 6; i++) {
    tft.setTextColor(wheel((uint8_t)(i * 40)), TFT_BLACK);
    tft.drawString("+ " + String(nv[i]), 24, 54 + i * 24, 2);
    beep(700 + i * 120, 40);
    uint32_t w = millis() + 90;
    while (millis() < w) { netPoll(); delay(5); }
  }
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Toque para continuar", 160, 222, 2);
  waitTap();
}

void splash() {
  tft.fillScreen(TFT_BLACK);
  for (int i = 0; i < 60; i++)
    tft.drawPixel(random(8, 312), random(8, 232), tft.color565(70, 70, 100));
  tft.drawRoundRect(2, 2, 316, 236, 12, TFT_WHITE);

  for (int i = 1; i <= 11; i++) {
    drawTitle(0, i);
    sndTone(300 + i * 60); delay(90); sndTone(0); delay(30);
  }

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("V9.1 | 24 jogos | niveis | celular e PC", 160, 146, 2);

  // 24 icones: duas fileiras de 12
  for (int i = 0; i < NGAMES; i++) {
    int col = (i < 12) ? i : i - 12;
    int x = 18 + col * 26;
    int y = (i < 12) ? 172 : 202;
    drawIcon(i, x, y);
    sndTone(450 + i * 40); delay(32); sndTone(0); delay(32);
  }
  const int jf[] = {784, 988, 1175, 1568};
  const int jd[] = {90, 90, 90, 220};
  playSeq(jf, jd, 4);

  musicPlay(0);
  int frame = 0;
  do {
    netPoll();
    readTouch();
    frame++;
    tft.drawRoundRect(2, 2, 316, 236, 12, wheel((uint8_t)(frame * 5)));
    if ((frame & 1) == 0) drawTitle(frame * 6, 11);
    uint8_t br2 = random(40, 255);
    tft.drawPixel(random(8, 312), random(6, 24), tft.color565(br2, br2, br2));
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(((frame / 10) & 1) ? TFT_WHITE : TFT_DARKGREY, TFT_BLACK);
    tft.drawString("TOQUE PARA COMECAR", 160, 228, 2);
    delay(40);
  } while (!anyConfirm());

  beep(1200, 80);
}

// =====================================================
//                    SETUP / LOOP
// =====================================================
void setup() {
  Serial.begin(115200);
  pinMode(BL_PIN, OUTPUT);
  digitalWrite(BL_PIN, HIGH);

  tft.init();
  tft.setRotation(1);
  tsSPI.begin(T_CLK, T_MISO, T_MOSI, T_CS);
  ts.begin(tsSPI);
  ts.setRotation(1);

  sndInit();
  randomSeed(esp_random());
  if (!LittleFS.begin(true)) Serial.println("LittleFS erro");

  prefs.begin("games", false);
  for (int i = 0; i < NGAMES; i++) {
    char key[6]; snprintf(key, sizeof(key), "hs%d", i);
    hs[i] = prefs.getInt(key, 0);
    snprintf(key, sizeof(key), "n%d", i);
    gname[i] = prefs.getString(key, defNames[i]);
  }
  menuBg = prefs.getUShort("mbg", 0);
  menuBgSet = prefs.getBool("mbs", false);
  bgOn = prefs.getBool("bgi", false) && LittleFS.exists("/bg.raw");
  accMode = constrain(prefs.getUChar("acc", 0), 0, 2);
  accPass = prefs.getString("apw", "");
  vol = constrain(prefs.getInt("vol", 2), 0, 5);   // padrao 2
  themeLight = prefs.getInt("thm", 0);
  musicOn = prefs.getInt("mus", 1);
  invertOn = prefs.getInt("inv", 0);
  gammaMode = constrain(prefs.getInt("gam", 1), 0, 2);
  batBeta = prefs.getBool("bat", false);
  batInit();
  batUpdate(true);
  bright = constrain(prefs.getInt("brt", 5), 1, 5);
  brightInit();
  diff = constrain(prefs.getInt("dif", 1), 0, 3);
  wifiSsid = prefs.getString("ws", WIFI_SSID_DEF);
  wifiPass = prefs.getString("wp", WIFI_PASS_DEF);
  setTheme();
  applyDisplay();

  netInit();                                        // WiFi do celular + site de controle

  // "super atualizacao" aparece sempre que a versao mudar (e pode ser vista de novo em CFG)
  if (prefs.getString("ver", "") != VERSION) {
    updateBanner();
    prefs.putString("ver", VERSION);
  }
  splash();
  menuDraw();
}

void loop() {
  netPoll();
  static uint32_t last = 0;
  if (millis() - last < 25) return;
  last = millis();

  readTouch();

  if (mode == MENU) {
    if (needRedraw) { needRedraw = false; menuDraw(); }
    menuStep();
    return;
  }

  // botao X (tela) ou botao MENU (controle) voltam ao menu
  static uint32_t exitArm = 0;
  if (exitArm && millis() > exitArm) {            // acabou o tempo: volta o HUD normal
    exitArm = 0;
    int *lp = lvLivesPtr();
    hud(gname[mode - 1].c_str(), lvScoreNow(), lp ? *lp : -1);
  }
  if ((pressed && tx >= 296 && ty < 22) || (padE & K_MENU)) {
    if (exitArm) {                                // segundo toque: sai
      exitArm = 0;
      toneDuty(0, 0);
      mode = MENU;
      menuDraw();
      return;
    }
    exitArm = millis() + 1600;                    // primeiro toque: so avisa
    tft.fillRect(0, 0, 296, 20, TFT_NAVY);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_YELLOW, TFT_NAVY);
    tft.drawString("Toque X de novo para sair", 4, 2, 2);
    beep(700, 40);
  }

  int m0 = mode;
  switch (mode) {
    case G_FLAPPY:  flappyStep();  break;
    case G_COBRA:   cobraStep();   break;
    case G_BLOCOS:  blocosStep();  break;
    case G_TOCA:    tocaStep();    break;
    case G_GENIUS:  geniusStep();  break;
    case G_CORRIDA: corridaStep(); break;
    case G_PONG:    pongStep();    break;
    case G_VELHA:   velhaStep();   break;
    case G_NAVE:    naveStep();    break;
    case G_TORRE:   torreStep();   break;
    case G_2048:    g2Step();      break;
    case G_TETRIS:  tetrisStep();  break;
    case G_LABIR:   labStep();     break;
    case G_FRUTAS:  frStep();      break;
    case G_CARROS:  carStep();     break;
    case G_MEMORIA: mmStep();      break;
    case G_REFLEXO: rfStep();      break;
    case G_SALTO:   sjStep();      break;
    case G_MINAS:   mnStep();      break;
    case G_ALVO:    tgStep();      break;
    case G_LIGA4:   c4Step();      break;
    case G_CONTAS:  ctStep();      break;
    case G_SURF:    surfStep();    break;
    case G_INVAS:   invStep();     break;
  }
  if (mode == m0) lvAfterStep();                    // bateu a meta? passa de nivel
}
