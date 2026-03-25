#pragma once
// this abstracts hardware into same functions across native/web/cardputer

#include <M5GFX.h>

// Key definitions (match M5Cardputer Keyboard_def.h values)
#ifndef KEY_LEFT_CTRL
#define KEY_LEFT_CTRL  0x80
#define KEY_LEFT_SHIFT 0x81
#define KEY_LEFT_ALT   0x82
#define KEY_FN         0xff
#define KEY_OPT        0x00
#define KEY_BACKSPACE  0x2a
#define KEY_TAB        0x2b
#define KEY_ENTER      0x28
#define KEY_ESC        0x29
// Arrow keys (HID usage codes, not present in Keyboard_def.h)
// On Cardputer: Fn + ; = right, Fn + , = left, Fn + . = down, Fn + ' = up
#define KEY_RIGHT      0x4f
#define KEY_LEFT       0x50
#define KEY_DOWN       0x51
#define KEY_UP         0x52
#endif


#ifdef SDL_h_
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

// SDL builds use a local "sdcard" directory as the root
#define SD_ROOT "sdcard"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
// Flush in-memory FS changes to IndexedDB
EM_JS(void, hal_fs_sync, (), {
  FS.syncfs(false, function(err) {
    if (err) console.warn('IDBFS sync error:', err);
  });
});
#else
static inline void hal_fs_sync() {}
#endif

// Prepend SD_ROOT to a path (caller must free or use a static buffer)
static void sd_path(char *out, size_t outsz, const char *path) {
  snprintf(out, outsz, "%s%s", SD_ROOT, path);
}

void listDir(const char *dirname, uint8_t levels) {
  char full[512];
  sd_path(full, sizeof(full), dirname);
  DIR *dir = opendir(full);
  if (!dir) { printf("Failed to open directory: %s\n", dirname); return; }
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (entry->d_name[0] == '.') continue; // skip . and ..
    char child[512];
    snprintf(child, sizeof(child), "%s/%s", dirname, entry->d_name);
    if (entry->d_type == DT_DIR) {
      printf("  DIR : %s\n", entry->d_name);
      if (levels) listDir(child, levels - 1);
    } else {
      char fullchild[512];
      sd_path(fullchild, sizeof(fullchild), child);
      struct stat st;
      stat(fullchild, &st);
      printf("  FILE: %s  SIZE: %ld\n", entry->d_name, (long)st.st_size);
    }
  }
  closedir(dir);
}

bool createDir(const char *path) {
  char full[512]; sd_path(full, sizeof(full), path);
  return mkdir(full, 0755) == 0;
}

bool removeDir(const char *path) {
  char full[512]; sd_path(full, sizeof(full), path);
  return rmdir(full) == 0;
}

bool readFile(const char *path) {
  char full[512]; sd_path(full, sizeof(full), path);
  FILE *f = fopen(full, "r");
  if (!f) return false;
  fclose(f);
  return true;
}

// Read entire file into buf (null-terminated). Returns false if not found.
bool hal_readFileStr(const char *path, char *buf, size_t bufsz) {
  char full[512]; sd_path(full, sizeof(full), path);
  FILE *f = fopen(full, "r");
  if (!f) return false;
  size_t n = fread(buf, 1, bufsz - 1, f);
  buf[n] = '\0';
  fclose(f);
  return true;
}

bool writeFile(const char *path, const char *message) {
  char full[512]; sd_path(full, sizeof(full), path);
  FILE *f = fopen(full, "w");
  if (!f) return false;
  bool ok = fputs(message, f) >= 0;
  fclose(f);
  hal_fs_sync();
  return ok;
}

bool appendFile(const char *path, const char *message) {
  char full[512]; sd_path(full, sizeof(full), path);
  FILE *f = fopen(full, "a");
  if (!f) return false;
  bool ok = fputs(message, f) >= 0;
  fclose(f);
  hal_fs_sync();
  return ok;
}

bool renameFile(const char *path1, const char *path2) {
  char full1[512], full2[512];
  sd_path(full1, sizeof(full1), path1);
  sd_path(full2, sizeof(full2), path2);
  return rename(full1, full2) == 0;
}

bool deleteFile(const char *path) {
  char full[512]; sd_path(full, sizeof(full), path);
  return remove(full) == 0;
}

bool testFileIO(const char *path) {
  char full[512]; sd_path(full, sizeof(full), path);
  static uint8_t buf[512];
  FILE *f = fopen(full, "w");
  if (!f) return false;
  for (int i = 0; i < 2048; i++) fwrite(buf, 1, sizeof(buf), f);
  fclose(f);
  return true;
}

#else
// Cardputer: wrap Arduino SD library
#include "M5Cardputer.h"
#include <SPI.h>
#include <SD.h>

#define SD_SPI_SCK_PIN  40
#define SD_SPI_MISO_PIN 39
#define SD_SPI_MOSI_PIN 14
#define SD_SPI_CS_PIN   12

void listDir(const char *dirname, uint8_t levels) {
  File root = SD.open(dirname);
  if (!root || !root.isDirectory()) return;
  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      if (levels) listDir(file.path(), levels - 1);
    }
    file = root.openNextFile();
  }
}

bool createDir(const char *path)               { return SD.mkdir(path); }
bool removeDir(const char *path)               { return SD.rmdir(path); }
bool renameFile(const char *path1, const char *path2) { return SD.rename(path1, path2); }
bool deleteFile(const char *path)              { return SD.remove(path); }

bool readFile(const char *path) {
  File file = SD.open(path);
  if (!file) return false;
  file.close();
  return true;
}

// Read entire file into buf (null-terminated). Returns false if not found.
bool hal_readFileStr(const char *path, char *buf, size_t bufsz) {
  File file = SD.open(path);
  if (!file) return false;
  size_t n = 0;
  while (file.available() && n < bufsz - 1) buf[n++] = file.read();
  buf[n] = '\0';
  file.close();
  return true;
}

bool writeFile(const char *path, const char *message) {
  File file = SD.open(path, FILE_WRITE);
  if (!file) return false;
  bool ok = file.print(message);
  file.close();
  return ok;
}

bool appendFile(const char *path, const char *message) {
  File file = SD.open(path, FILE_APPEND);
  if (!file) return false;
  bool ok = file.print(message);
  file.close();
  return ok;
}

bool testFileIO(const char *path) {
  static uint8_t buf[512];
  File file = SD.open(path, FILE_WRITE);
  if (!file) return false;
  for (int i = 0; i < 2048; i++) file.write(buf, sizeof(buf));
  file.close();
  return true;
}
#endif

// ---------------------------------------------------------------------------
// LoRa + GPS
// ---------------------------------------------------------------------------

#ifdef SDL_h_

#include <functional>

// --- LoRa stub ---
// Incoming bytes are injected from JS via hal_loraInject (exported below).
// A small ring buffer holds pending packets until hal_loop drains them.

static std::function<void(uint8_t*, int)> _lora_cb;
static uint8_t  _lora_buf[256];
static int      _lora_len = 0;
static bool     _lora_pending = false;

void hal_onLoraMessage(std::function<void(uint8_t*, int)> cb) { _lora_cb = cb; }

bool hal_sendLoraMessage(uint8_t *data, int len) {
#ifdef __EMSCRIPTEN__
  // Expose outgoing bytes to JS as a Uint8Array on window.loraOut
  EM_ASM({
    if (Module.loraOut) Module.loraOut(Array.from(Module.HEAPU8.subarray($0, $0 + $1)));
  }, data, len);
#endif
  return true;
}

// Called from JS: Module._hal_loraInject(ptr, len) — or use the helper below
extern "C" void hal_loraInject(uint8_t *data, int len) {
  if (len > (int)sizeof(_lora_buf)) len = sizeof(_lora_buf);
  memcpy(_lora_buf, data, len);
  _lora_len = len;
  _lora_pending = true;
}

// --- GPS stub ---
static double _gps_lat = 0.0, _gps_lng = 0.0;
static bool   _gps_valid = false;

bool hal_getLocation(double *lat, double *lng) {
  if (!_gps_valid) return false;
  *lat = _gps_lat;
  *lng = _gps_lng;
  return true;
}

int hal_gpsSatellites() {
#ifdef __EMSCRIPTEN__
  return EM_ASM_INT({ return Module.satelliteCount || 0; });
#else
  return 0;
#endif
}

// Called from JS: Module._hal_gpsSetLocation(lat_double, lng_double)
extern "C" void hal_gpsSetLocation(double lat, double lng) {
  _gps_lat = lat;
  _gps_lng = lng;
  _gps_valid = true;
}

#ifdef __EMSCRIPTEN__
// Expose JS-callable helpers on the window object so the browser console can use them:
//   window.loraInject([0x48,0x65,0x6c,0x6c,0x6f])
//   window.gpsSet(51.5074, -0.1278)
EM_JS(void, hal_js_helpers, (), {
  Module.loraInject = function(bytes) {
    var buf = Module._malloc(bytes.length);
    Module.HEAPU8.set(bytes, buf);
    Module._hal_loraInject(buf, bytes.length);
    Module._free(buf);
  };
  Module.loraOut = null;      // set to a function to receive outgoing bytes
  Module.satelliteCount = 0;  // set to simulate satellite count
  Module.gpsSet = function(lat, lng) {
    Module._hal_gpsSetLocation(lat, lng);
  };
});
#endif

#else
// ---------------------------------------------------------------------------
// Cardputer hardware: RadioLib SX1262 + MultipleSatellite (TinyGPSPlus)
// ---------------------------------------------------------------------------
#include <RadioLib.h>
#include <MultipleSatellite.h>
#include <functional>

#define LORA_BW           125.0f
#define LORA_SF           12
#define LORA_CR           5
#define LORA_FREQ         868.0
#define LORA_SYNC_WORD    0x34
#define LORA_TX_POWER     22
#define LORA_PREAMBLE_LEN 20

// NSS, IRQ, RST, BUSY
static SX1262 _lora_radio = new Module(GPIO_NUM_5, GPIO_NUM_4, GPIO_NUM_3, GPIO_NUM_6);
static bool _lora_ok = false;
static volatile bool _lora_rx_flag = false;
static volatile bool _lora_tx_flag = false;
static std::function<void(uint8_t*, int)> _lora_cb;

ICACHE_RAM_ATTR static void _lora_rx_isr() { _lora_rx_flag = true; }
ICACHE_RAM_ATTR static void _lora_tx_isr() { _lora_tx_flag = true; }

void hal_onLoraMessage(std::function<void(uint8_t*, int)> cb) { _lora_cb = cb; }

bool hal_sendLoraMessage(uint8_t *data, int len) {
  if (!_lora_ok) return false;
  return _lora_radio.transmit(data, len) == RADIOLIB_ERR_NONE;
}

// GPS: Serial1, RX=15, TX=13
static MultipleSatellite _gps(Serial1, 115200, SERIAL_8N1, 15, 13);
static bool _gps_started = false;

bool hal_getLocation(double *lat, double *lng) {
  if (!_gps_started || !_gps.location.isValid()) return false;
  *lat = _gps.location.lat();
  *lng = _gps.location.lng();
  return true;
}

int hal_gpsSatellites() {
  if (!_gps_started) return 0;
  return (int)_gps.satellites.value();
}
#endif

// ---------------------------------------------------------------------------
// Shared LoRa/GPS poll — called from hal_loop()
// ---------------------------------------------------------------------------
static void _hal_lora_gps_poll() {
#ifdef SDL_h_
  // Drain any injected LoRa packet
  if (_lora_pending && _lora_cb) {
    _lora_pending = false;
    _lora_cb(_lora_buf, _lora_len);
  }
#else
  // Drain incoming LoRa packets
  if (_lora_rx_flag) {
    _lora_rx_flag = false;
    int len = _lora_radio.getPacketLength();
    if (len > 0 && _lora_cb) {
      uint8_t buf[256];
      if (len > (int)sizeof(buf)) len = sizeof(buf);
      if (_lora_radio.readData(buf, len) == RADIOLIB_ERR_NONE) {
        _lora_cb(buf, len);
      }
    }
    _lora_radio.startReceive();
  }
  if (_lora_tx_flag) {
    _lora_tx_flag = false;
    _lora_radio.startReceive();
  }
  // Update GPS
  if (_gps_started) _gps.updateGPS();
#endif
}

static M5GFX _display;
static M5Canvas gfx(&_display);

static bool _sd_ok = false;
bool hal_sdOk() { return _sd_ok; }

bool hal_setup() {
#ifndef SDL_h_
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  _display = (M5GFX)M5Cardputer.Display;
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);
  SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
  _sd_ok = SD.begin(SD_SPI_CS_PIN, SPI, 25000000);
  // LoRa init
  if (_lora_radio.begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR, LORA_SYNC_WORD,
                        LORA_TX_POWER, LORA_PREAMBLE_LEN, 3.0, true) == RADIOLIB_ERR_NONE) {
    _lora_radio.setCurrentLimit(140);
    _lora_radio.setPacketReceivedAction(_lora_rx_isr);
    _lora_radio.setPacketSentAction(_lora_tx_isr);
    _lora_radio.startReceive();
    _lora_ok = true;
  }
  // GPS init — hot start reuses cached almanac for faster fix
  _gps.begin();
  _gps.setSystemBootMode(BOOT_HOST_START);
  _gps_started = true;
#else
  _display.init();
  // Require Ctrl+Shift for Panel_sdl rotate/scale shortcuts so normal key
  // presses (L, R, 1-6) don't accidentally trigger them.
  lgfx::Panel_sdl::setShortcutKeymod((SDL_Keymod)(KMOD_CTRL | KMOD_SHIFT));
  mkdir(SD_ROOT, 0755); // ensure sdcard dir exists
  _sd_ok = true;
#ifdef __EMSCRIPTEN__
  hal_js_helpers();
#endif
#endif
  gfx.setColorDepth(16);
  gfx.createSprite(_display.width(), _display.height());

  return true;
}

void hal_loop() {
#ifndef SDL_h_
  M5Cardputer.update();
#endif
  _hal_lora_gps_poll();
  gfx.pushSprite(&_display, 0, 0);
}

// Returns true while the key is held.
// c can be a printable char ('a','A','1','!',...) or a KEY_* constant.
bool hal_isKeyPressed(char c) {
#ifdef SDL_h_
  const uint8_t* keys = SDL_GetKeyboardState(NULL);

  // ESC
  if ((uint8_t)c == KEY_ESC) {
    return keys[SDL_SCANCODE_ESCAPE];
  }

  bool shift_held = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];

  // Special keys by HID code
  switch ((uint8_t)c) {
    case KEY_BACKSPACE:  return keys[SDL_SCANCODE_BACKSPACE];
    case KEY_TAB:        return keys[SDL_SCANCODE_TAB];
    case KEY_ENTER:      return keys[SDL_SCANCODE_RETURN];
    case KEY_LEFT_CTRL:  return keys[SDL_SCANCODE_LCTRL]  || keys[SDL_SCANCODE_RCTRL];
    case KEY_LEFT_SHIFT: return keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
    case KEY_LEFT_ALT:   return keys[SDL_SCANCODE_LALT]   || keys[SDL_SCANCODE_RALT];
    case KEY_RIGHT:      return keys[SDL_SCANCODE_RIGHT];
    case KEY_LEFT:       return keys[SDL_SCANCODE_LEFT];
    case KEY_DOWN:       return keys[SDL_SCANCODE_DOWN];
    case KEY_UP:         return keys[SDL_SCANCODE_UP];
    // KEY_OPT (0x00) and KEY_FN (0xff) have no SDL equivalent — treat as not pressed
    case KEY_OPT:        return false;
    case KEY_FN:         return false;
    default:             break;
  }

  // Shifted symbols: map to the unshifted key + require shift held.
  // SDL_GetScancodeFromKey only knows about unshifted keycodes, so we
  // manually map the common shifted symbols from the Cardputer layout.
  switch (c) {
    case '!': return keys[SDL_SCANCODE_1]          && shift_held;
    case '@': return keys[SDL_SCANCODE_2]          && shift_held;
    case '#': return keys[SDL_SCANCODE_3]          && shift_held;
    case '$': return keys[SDL_SCANCODE_4]          && shift_held;
    case '%': return keys[SDL_SCANCODE_5]          && shift_held;
    case '^': return keys[SDL_SCANCODE_6]          && shift_held;
    case '&': return keys[SDL_SCANCODE_7]          && shift_held;
    case '*': return keys[SDL_SCANCODE_8]          && shift_held;
    case '(': return keys[SDL_SCANCODE_9]          && shift_held;
    case ')': return keys[SDL_SCANCODE_0]          && shift_held;
    case '_': return keys[SDL_SCANCODE_MINUS]      && shift_held;
    case '+': return keys[SDL_SCANCODE_EQUALS]     && shift_held;
    case '~': return keys[SDL_SCANCODE_GRAVE]      && shift_held;
    case '{': return keys[SDL_SCANCODE_LEFTBRACKET]  && shift_held;
    case '}': return keys[SDL_SCANCODE_RIGHTBRACKET] && shift_held;
    case '|': return keys[SDL_SCANCODE_BACKSLASH]  && shift_held;
    case ':': return keys[SDL_SCANCODE_SEMICOLON]  && shift_held;
    case '"': return keys[SDL_SCANCODE_APOSTROPHE] && shift_held;
    case '<': return keys[SDL_SCANCODE_COMMA]      && shift_held;
    case '>': return keys[SDL_SCANCODE_PERIOD]     && shift_held;
    case '?': return keys[SDL_SCANCODE_SLASH]      && shift_held;
    default:  break;
  }

  // Printable ASCII (unshifted): letters and unshifted symbols.
  SDL_Keycode kc;
  if (c >= 'A' && c <= 'Z') {
    kc = (SDL_Keycode)((c - 'A') + 'a');
    return keys[SDL_GetScancodeFromKey(kc)] && shift_held;
  }
  kc = (SDL_Keycode)c;
  SDL_Scancode sc = SDL_GetScancodeFromKey(kc);
  if (sc == SDL_SCANCODE_UNKNOWN) return false;
  return keys[sc];

#else
  // Cardputer hardware path
  bool fn = M5Cardputer.Keyboard.keysState().fn;
  switch ((uint8_t)c) {
    case KEY_ESC:   return fn && M5Cardputer.Keyboard.isKeyPressed('`');
    case KEY_UP:    return fn && M5Cardputer.Keyboard.isKeyPressed(';');
    case KEY_LEFT:  return fn && M5Cardputer.Keyboard.isKeyPressed(',');
    case KEY_RIGHT: return fn && M5Cardputer.Keyboard.isKeyPressed('/');
    case KEY_DOWN:  return fn && M5Cardputer.Keyboard.isKeyPressed('.');
    // Suppress Fn-combo base keys when Fn is held
    case '`': return !fn && M5Cardputer.Keyboard.isKeyPressed('`');
    case ';': return !fn && M5Cardputer.Keyboard.isKeyPressed(';');
    case ',': return !fn && M5Cardputer.Keyboard.isKeyPressed(',');
    case '/': return !fn && M5Cardputer.Keyboard.isKeyPressed('/');
    case '.': return !fn && M5Cardputer.Keyboard.isKeyPressed('.');
    default:  return M5Cardputer.Keyboard.isKeyPressed(c);
  }
#endif
}

void setup(void);
void loop(void);

#if __EMSCRIPTEN__
#include <emscripten.h>
static void em_loop(void) {
  loop();
  lgfx::Panel_sdl::loop();
}
int main(int, char**) {
  lgfx::Panel_sdl::setup();
  setup();
  emscripten_set_main_loop(em_loop, 0, 1);
  return 0;
}

#elif defined ( SDL_h_ )
__attribute__((weak))
int user_func(bool* running) {
  setup();
  while(*running) {
    loop();
  }
  return 0;
}
int main(int, char**) {
  return lgfx::Panel_sdl::main(user_func, 128);
}
#elif defined ( ESP_PLATFORM ) && !defined ( ARDUINO )
extern "C" {
  int app_main(int, char**) {
    setup();
    while(true) {
      loop();
    }
    return 0;
  }
}
#endif
