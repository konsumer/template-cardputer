#include "hal.h"

int pos_x, pos_y;

// wrap-around counter
class Counter {
public:
  int val;
  int size;
  Counter(int size = 1, int val = 0) : size(size), val(val) {}
  int increment(int c = 1) { return val = ((val + c) % size + size) % size; }
  int decrement(int c = 1) { return val = ((val - c) % size + size) % size; }
  
  operator int() const { return val; }

  // prefix ++c / --c
  Counter& operator++() { val = ((val + 1) % size + size) % size; return *this; }
  Counter& operator--() { val = ((val - 1) % size + size) % size; return *this; }
  // postfix c++ / c-- (dummy int parameter distinguishes from prefix)
  Counter operator++(int) { Counter tmp = *this; ++(*this); return tmp; }
  Counter operator--(int) { Counter tmp = *this; --(*this); return tmp; }
  // c += n / c -= n
  Counter& operator+=(int c) { val = ((val + c) % size + size) % size; return *this; }
  Counter& operator-=(int c) { val = ((val - c) % size + size) % size; return *this; }
};


class Tab {
public:
  const char* name;
  Tab(const char* name) : name(name) {}

  virtual void setup() {}  
  virtual void update() {}
  virtual ~Tab() {}
};


class TabKeys : public Tab {
public:
  TabKeys() : Tab("Keys") {}

  void setup() override {
    gfx.setTextSize(4.0);
    pos_x = gfx.width() / 2;
    pos_y = gfx.height() / 2 - 15;
  }
  
  void update() override {
    gfx.clear(BLUE);
    gfx.setTextSize(4.0);
    gfx.setTextColor(WHITE);

    // special keys: check first, display multi-char label
    static const struct { uint8_t key; const char* label; } specials[] = {
      { KEY_UP,        "UP"        },
      { KEY_DOWN,      "DOWN"      },
      { KEY_LEFT,      "LEFT"      },
      { KEY_RIGHT,     "RIGHT"     },
      { KEY_LEFT_CTRL, "CTRL"      },
      { KEY_LEFT_ALT,  "ALT"       },
      { KEY_OPT,       "OPT"       },
      { KEY_BACKSPACE, "BACKSPACE" },
      { KEY_TAB,       "TAB"       },
      { KEY_ENTER,     "ENTER"     },
      { KEY_ESC,       "ESC"       },
      { ' ',           "SPACE"     },
    };
    for (auto& s : specials) {
      if (hal_isKeyPressed(s.key)) {
        gfx.drawCenterString(s.label, pos_x, pos_y);
        return;
      }
    }

    // printable keys: display the character itself
    static const char printable[] =
      "abcdefghijklmnopqrstuvwxyz"
      "1234567890"
      "`-=[]\\;',./"
      "~_+{}|:\"<>?!@#$%^&*()";
    for (char c : printable) {
      if (hal_isKeyPressed(c)) {
        char buf[2] = { c, '\0' };
        gfx.drawCenterString(buf, pos_x, pos_y);
        return;
      }
    }
  }
private:
  int pos_x, pos_y;
};


class TabGraphics : public Tab {
public:
  TabGraphics() : Tab("Graphics") {}

  void setup() override {}

  void update() override {
    int w = gfx.width();
    int h = gfx.height() - 16; // leave room for tab bar

    // random color helper: independent R/G/B channels
    auto rc = []() -> uint32_t { return gfx.color888(rand() % 256, rand() % 256, rand() % 256); };

    switch (rand() % 5) {
      case 0: // filled circle
        gfx.fillCircle(rand() % w, rand() % h, rand() % 20 + 4, rc());
        break;
      case 1: // filled rect
        gfx.fillRect(rand() % w, rand() % h, rand() % 40 + 4, rand() % 40 + 4, rc());
        break;
      case 2: // filled round rect
        gfx.fillRoundRect(rand() % w, rand() % h, rand() % 40 + 8, rand() % 30 + 8, 4, rc());
        break;
      case 3: // filled triangle
        gfx.fillTriangle(rand() % w, rand() % h, rand() % w, rand() % h, rand() % w, rand() % h, rc());
        break;
      case 4: // line
        gfx.drawLine(rand() % w, rand() % h, rand() % w, rand() % h, rc());
        break;
    }
  }
};

class TabCounter : public Tab {
public:
  TabCounter() : Tab("SD Counter") {}

  void setup() override {
    if (!hal_sdOk()) return;
    char buf[32] = "0";
    hal_readFileStr("/counter.txt", buf, sizeof(buf));
    _count = atoi(buf) + 1;
    char out[32];
    snprintf(out, sizeof(out), "%d", _count);
    writeFile("/counter.txt", out);
  }

  void update() override {
    gfx.clear(BLACK);
    gfx.setTextColor(WHITE);

    if (!hal_sdOk()) {
      gfx.setTextSize(1.5);
      gfx.drawCenterString("No SD card", gfx.width() / 2, gfx.height() / 2 - 16);
      return;
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "%d", _count);
    gfx.setTextSize(4.0);
    gfx.drawCenterString(buf, gfx.width() / 2, gfx.height() / 2 - 24);
  }

private:
  int _count = 0;

  void _save() {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", _count);
    writeFile("/counter.txt", buf);
  }
};

std::vector<Tab*> tabs = { new TabKeys(), new TabGraphics(), new TabCounter() };
Counter currentTab(tabs.size());

void setup(void) {
  hal_setup();
  tabs[currentTab]->setup();
}

void loop(void) {
  hal_loop();
  tabs[currentTab]->update();

  // draw tabs
  gfx.setTextSize(1.0);
  int count = tabs.size();
  int w = gfx.width() / count;
  int h = 16;
  int y = gfx.height() - h;
  for (int i = 0; i < count; i++) {
    int x = i * w;
    bool active = i == currentTab;
    gfx.fillRoundRect(x + 1, y, w - 2, h - 2, 3, active ? WHITE : DARKGREY);
    gfx.setTextColor(active ? BLACK : WHITE);
    gfx.drawCenterString(tabs[i]->name, x + w / 2, y + 4);
  }

  // left/right switches tabs
  bool left  = hal_isKeyPressed(KEY_LEFT);
  bool right = hal_isKeyPressed(KEY_RIGHT);
  static bool prevLeft  = false;
  static bool prevRight = false;
  if (left && !prevLeft) {
    currentTab--;
    tabs[currentTab]->setup();
  }
  if (right && !prevRight) {
    currentTab++;
    tabs[currentTab]->setup();
  }
  prevLeft  = left;
  prevRight = right;
}
