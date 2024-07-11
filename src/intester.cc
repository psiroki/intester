#include <sstream>
#include <iostream>
#include <cstring>

#include "sdlcompat.hh"
#include "font.h"

SDL_Color color = {0xb5, 0x7e, 0xdc}; // Lavender color

VideoSurface* screen;
TTF_Font* font;

const int TYPE_KEY = 0 << 16;
const int TYPE_BUTTON = 1 << 16;
const int TYPE_HAT = 2 << 16;

uint64_t nextSeed(uint64_t seed) {
  return (seed * 0x5DEECE66DLL + 0xBLL) & ((1LL << 48) - 1);
}

float seedToFloat(uint64_t seed) {
  return (seed & 0xffffff) / static_cast<float>(0xffffff);
}

class Random {
  uint64_t seed;
public:
  Random(uint64_t seed);

  uint64_t operator()();

  uint64_t operator()(int n);

  float fraction();
} commonRandom(7773);

Random::Random(uint64_t seed): seed(nextSeed(seed)) {}

uint64_t Random::operator()() {
  return seed = nextSeed(seed);
}

uint64_t Random::operator()(int n) {
  return (seed = nextSeed(seed)) % n;
}

float Random::fraction() {
  return seedToFloat(seed = nextSeed(seed));
}

class FixedColor {
  uint32_t r;
  uint32_t g;
  uint32_t b;

  static uint32_t addNoise(uint32_t val, int32_t noise) {
    return noise < 0 && val < -noise ? 0 : (val >= 256*65536-noise ? 255*65536 : val + noise);
  }
public:
  FixedColor() {}
  FixedColor(uint32_t r, uint32_t g, uint32_t b): r(r), g(g), b(b) {}
  FixedColor(SDL_Color col): r(col.r << 16), g(col.g << 16), b(col.b << 16) {
  }

  FixedColor& operator +=(const FixedColor &other) {
    r += other.r;
    g += other.g;
    b += other.b;
    return *this;
  }


  FixedColor& operator -=(const FixedColor &other) {
    r -= other.r;
    g -= other.g;
    b -= other.b;
    return *this;
  }

  FixedColor& operator /=(uint32_t div) {
    r /= div;
    g /= div;
    b /= div;
    return *this;
  }

  operator SDL_Color() {
    SDL_Color result {
      static_cast<uint8_t>(r >> 16),
      static_cast<uint8_t>(g >> 16),
      static_cast<uint8_t>(b >> 16),
      0xff
    };
    return result;
  }

  SDL_Color withNoise() {
    int noise = (commonRandom() & 0xFFFF) - 0x8000;
    return SDL_Color {
      static_cast<uint8_t>(addNoise(r, noise) >> 16),
      static_cast<uint8_t>(addNoise(g, noise) >> 16),
      static_cast<uint8_t>(addNoise(b, noise) >> 16),
      0xff
    };
  }
};

class FixedGradient {
  FixedColor pos;
  FixedColor step;
public:
  FixedGradient(SDL_Color start, SDL_Color end, uint32_t steps): pos(start), step(end) {
    step -= pos;
    step /= steps;
  }

  SDL_Color current() {
    return pos;
  }

  SDL_Color dithered() {
    return pos.withNoise();
  }

  void stepNext() {
    pos += step;
  }
};

class KeyDisplay {
  Video &video;
  const char **keys;
  int numKeys;
  bool *buttons;
  int numButtons, maxButtons;
  int axes[256];
  int numAxes;
  int mouseX, mouseY;
  int numHats;

  void drawText(const char *text, int offset);
public:
  inline KeyDisplay(Video &video, const char **keys, int numKeys, bool *buttons, int numButtons, int numAxes, int numHats):
      video(video),
      keys(keys), numKeys(numKeys),
      buttons(buttons), numButtons(numButtons), maxButtons(0), numAxes(numAxes), numHats(numHats) {
    memset(axes, 0, sizeof(axes));
    mouseX = video.getScreen()->getWidth() * 2;
    mouseY = video.getScreen()->getHeight() * 2;
  }
  void displayString(const char *text, float progress, int hat);
  inline void setMaxButtons(int val) {
    maxButtons = val;
  }
  inline void setAxis(int index, int value) {
    axes[index] = value;
  }
  inline void setMouseMovement(int x, int y) {
    mouseX = x;
    mouseY = y;
  }
};

void KeyDisplay::drawText(const char *text, int offset) {
  VideoSurface *textSurface = video.adapt(TTF_RenderText_Blended(font, text, color));

  if (textSurface) {
    VideoSurface* rotatedSurface = NULL;
    
#ifdef FLIP
    // Create a new surface for the rotated text
    rotatedSurface = video.createSurface(textSurface->w, textSurface->h);

    if (rotatedSurface) {
      LockedSurface ts;
      textSurface->lock(&ts);
      LockedSurface rs;
      rotatedSurface->lock(&rs);
      // Rotate the text 180 degrees
      for (int y = 0; y < ts.h; ++y) {
        Uint32 *src = ((Uint32*)ts.pixels) + (y * ts.pitch / 4);
        Uint32 *dst = ((Uint32*)rs.pixels) + ((ts.h - y - 1) * rs.pitch / 4 + ts.w);
        for (int x = 0; x < ts.w; ++x) {
          *--dst = *src++;
        }
      }
      textSurface->unlock();
      rotatedSurface->unlock();
    }

    const int nominator = 1;
#else
    const int nominator = 3;
#endif

    int textLocation[2] = {
      (screen->getWidth() * nominator / 2 - textSurface->getWidth()) / 2,
      (screen->getHeight() * nominator / 2 - textSurface->getHeight()) / 2 + offset,
    };

    if (rotatedSurface) {
      rotatedSurface->blitOn(screen, textLocation[0], textLocation[1]);
      delete rotatedSurface;
    } else {
      textSurface->blitOn(screen, textLocation[0], textLocation[1]);
    }

    delete textSurface;
  }
}

void KeyDisplay::displayString(const char *text, float progress, int hat) {
  if (!text) text = "";
  // if (*text) std::cout << text << std::endl;
  if (!*text) {
    for (int i = numKeys - 1; i >= 0; --i) {
      if (keys[i]) {
        text = keys[i];
        break;
      }
    }
  }
  int lenChecked = strnlen(text, 255) + 1;

  for (int i = maxButtons; i < numButtons; ++i) {
    if (buttons[i]) maxButtons = i + 1;
  }

  int directions[] = { 1, 5, 7, 3 };

  FixedGradient bgGrad(SDL_Color { 0, 0, 0 }, color, screen->getHeight() * 3);
  for (int y = 0; y < screen->getHeight(); ++y) {
    SDL_Color col(bgGrad.dithered());
    bgGrad.stepNext();
    screen->fill(0, y, screen->getWidth(), 1, (255u << 24)|col.b|(col.g << 8)|(col.r << 16));
  }

  uint32_t mainColor = (255u << 24)|color.b|(color.g << 8)|(color.r << 16);

  int width = progress * screen->getWidth();
  screen->fill((screen->getWidth() - width) >> 1,
#ifdef FLIP
    0,
#else
    screen->getHeight() - 8,
#endif
    width,
    8,
    mainColor
  );

  int rw = (screen->getWidth() - 32) / 16;
  int rh = (screen->getHeight() - 32) / 16;
  if (rw < rh) {
    rh = rw;
  } else {
    rw = rh;
  }

  for (int i = 0; i < maxButtons; ++i) {
    int x = 8 + rw * (i & 15);
    int y = 8 + rh * (i >> 4);
    int w = rw * 7 / 8;
    int h = rh * 7 / 8;
    if (buttons[i]) {
      screen->fill(x, y, w, h, mainColor);
    } else {
      screen->fill(x, y, 1, h, mainColor);
      screen->fill(x, y, w, 1, mainColor);
      screen->fill(x+w-1, y, 1, h, mainColor);
      screen->fill(x, y+h-1, w, 1, mainColor);
    }
  }

  if (numHats > 0) {
    for (int i = 0; i < 4; ++i) {
      int index = directions[i];
      int x = rw * (index % 3) + screen->getWidth() - rw * 3 - 8;
      int y = 8 + rh * (index / 3);
      int w = rw + 1;
      int h = rh + 1;
      if ((hat >> i) & 1) {
        screen->fill(x, y, w, h, mainColor);
      } else {
        screen->fill(x, y, 1, h, mainColor);
        screen->fill(x, y, w, 1, mainColor);
        screen->fill(x+w-1, y, 1, h, mainColor);
        screen->fill(x, y+h-1, w, 1, mainColor);
      }
    }
  }

  int numRows = (numAxes + 7) / 8;
  int numCols = numAxes < 8 ? numAxes / 2 : 4;
  for (int i = 0; i < numAxes; i += 2) {
    int w = rw * 15 / 8;
    int h = rh * 15 / 8;
    int x = (screen->getWidth() / 2 - numCols * rw) / 2 + rw * (i & 7);
    int y = (screen->getHeight() * 3 / 4) - numRows * rh + 2 * rh * (i >> 3);
    int dx = (axes[i] * w / 32768 + w) / 2;
    int dy = (axes[i + 1] * h / 32768 + h) / 2;
    screen->fill(x, y, 1, h, mainColor);
    screen->fill(x, y, w, 1, mainColor);
    screen->fill(x+w-1, y, 1, h, mainColor);
    screen->fill(x, y+h-1, w, 1, mainColor);

    screen->fill(x + dx - rw / 8, y + dy - rh / 8, rw / 4, rh / 4, mainColor);
  }

  int mx = screen->getWidth() / 2 + mouseX;
  int my = screen->getHeight() / 2 + mouseY;
  screen->fill(mx - 4, my, 9, 1, mainColor);
  screen->fill(mx, my - 4, 1, 9, mainColor);

  int keysDown = 0;
  for (int i = 0; i < numKeys; ++i) {
    if (keys[i] && strncmp(text, keys[i], lenChecked) != 0) {
      ++keysDown;
      if (keysDown >= 2) break;
    }
  }
  int y = (keysDown + 1) * -16;
  for (int i = 0; i < numKeys; ++i) {
    if (keys[i] && strncmp(text, keys[i], lenChecked) != 0) {
      drawText(keys[i], y);
      y += 32;
      --keysDown;
      if (keysDown <= 0) break;
    }
  }
  drawText(text, y);

  video.present();
}

int main(int argc, char* argv[]) {
  // Initialize SDL video and joystick
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
    perror("Cannot initialize SDL");
    return 1;
  }

  SDL_ShowCursor(false);
#ifdef USE_SDL2
  SDL_SetRelativeMouseMode(SDL_TRUE);
#else
  SDL_WM_GrabInput(SDL_GRAB_ON);
#endif
  
  SDL_JoystickEventState(SDL_ENABLE);
  std::cout << "Number of joysticks: " << SDL_NumJoysticks() << std::endl;
  SDL_Joystick *joy = SDL_JoystickOpen(0);

  if (TTF_Init() < 0) {
    perror("Can't initialize SDL_TTF");
    SDL_Quit();
    return 2;
  }

  // Set video mode
#ifdef PORTRAIT
  Video video(640, 480, 3);
#else
  Video video(640, 480);
#endif
  screen = video.getScreen();
  if (!screen) {
    perror("Can't set video mode");
    TTF_Quit();
    SDL_Quit();
    return 3;
  }

  SDL_RWops *ttf = SDL_RWFromConstMem(RussoOne_Regular_ttf, RussoOne_Regular_ttf_len);
  // Load font (the second parameter value will tell to free the rwops instance)
  font = TTF_OpenFontRW(ttf, 1, 32);
  if (!font) {
    perror("Can't load font");
    SDL_Quit();
    TTF_Quit();
    return 4;
  }

  SDL_Event event;
  bool running = true;
  bool joyButtons[256];
  const char* keys[NUM_SCANCODES];
  int lastHat = 0;
  memset(joyButtons, 0, sizeof(joyButtons));
  memset(keys, 0, sizeof(keys));

  int lastDown = 0, downStride = 0;

  KeyDisplay kd(video, keys, sizeof(keys)/sizeof(*keys), joyButtons, sizeof(joyButtons) / sizeof(*joyButtons), SDL_JoystickNumAxes(joy), SDL_JoystickNumHats(joy));
  kd.setMaxButtons(SDL_JoystickNumButtons(joy));
  kd.displayString("", 0.0f, 0);
  std::string textToDisplay;
  bool eventPending = false;
  bool needUpdate = false;
  while (running && (eventPending || SDL_WaitEvent(&event))) {
    eventPending = false;
    video.present();
    int downCode = 0;
    if (event.type == SDL_QUIT) {
      running = false;
    } else if (event.type == SDL_MOUSEMOTION) {
      kd.setMouseMovement(event.motion.xrel, event.motion.yrel);
      needUpdate = true;
    } else if (event.type == SDL_JOYAXISMOTION) {
      SDL_JoyAxisEvent &axisEvent(event.jaxis);
      kd.setAxis(axisEvent.axis, axisEvent.value);
      needUpdate = true;
    } else if (event.type == SDL_JOYHATMOTION) {
      SDL_JoyHatEvent &hat(event.jhat);
      if (hat.value) {
        const char *upDown = hat.value & SDL_HAT_UP ? "up" : hat.value & SDL_HAT_DOWN ? "down" : nullptr;
        const char *leftRight = hat.value & SDL_HAT_LEFT ? "left" : hat.value & SDL_HAT_RIGHT ? "right" : nullptr;
        std::stringstream hatName;
        hatName << "Hat ";
        if (!upDown && !leftRight) {
          hatName << "centered";
        } else {
          if (upDown) {
            hatName << upDown;
            if (leftRight) hatName << " ";
          }
          if (leftRight) hatName << leftRight;
        }
        textToDisplay = hatName.str();
      }
      if (hat.value != lastHat) {
        lastHat = hat.value;
        if (hat.value) downCode = TYPE_HAT | hat.value;
        needUpdate = true;
      }
    } else if (event.type == SDL_JOYBUTTONDOWN) {
      char buttonName[256] = { 0 };
      int button = event.jbutton.button;
      snprintf(buttonName, sizeof(buttonName), "Button #%d", button);
      textToDisplay = buttonName;
      needUpdate = true;
      if (!joyButtons[button]) {
        joyButtons[button] = true;
        downCode = TYPE_BUTTON | button;
      }
    } else if (event.type == SDL_JOYBUTTONUP) {
      int button = event.jbutton.button;
      if (joyButtons[button]) {
        joyButtons[button] = false;
        needUpdate = true;
      }
    } else if (event.type == SDL_KEYDOWN) {
      int key = keyCodeFromEvent(event);
      const char *keyName = SDL_GetKeyName(event.key.keysym.sym);
      if (!keys[key]) {
        keys[key] = keyName;
        downCode = TYPE_KEY | key;
      }
      textToDisplay = keyName;
      needUpdate = true;
    } else if (event.type == SDL_KEYUP) {
      int key = keyCodeFromEvent(event);
      if (keys[key]) {
        keys[key] = nullptr;
        needUpdate = true;
      }
    }
    if (downCode) {
      if (lastDown == downCode) {
        ++downStride;
      } else {
        downStride = 1;
      }
      lastDown = downCode;
    }
    if (downStride == 3) running = false;
    if (SDL_PollEvent(&event)) {
      eventPending = true;
    } else {
      // only repaint when idle
      if (needUpdate) {
        int ds = downStride - 1;
        if (ds < 0) ds = 0;
        kd.displayString(textToDisplay.c_str(), ds / 2.0f, lastHat);
        needUpdate = false;
      }
    }
  }

  // Clean up
  TTF_CloseFont(font);
  TTF_Quit();
//  SDL_Quit();

  return 0;
}
