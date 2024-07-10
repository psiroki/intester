#pragma once

#include <stdint.h>

struct LockedSurface {
  uint8_t *pixels;
  int w;
  int h;
  int pitch;
};

#ifdef USE_SDL2
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

class Video;

class VideoSurface {
  SDL_Surface *surface;
  SDL_Texture *texture;
  int width, height;
  Video *video;
protected:
  friend Video;
  VideoSurface(Video *video, SDL_Surface *surface, SDL_Texture *texture, int w, int h);
  void update();
public:
  ~VideoSurface();

  inline int getWidth() { return width; }
  inline int getHeight() { return height; }

  void fill(uint32_t color);
  void fill(int x, int y, int w, int h, uint32_t color);
  int lock(LockedSurface *locked);
  void unlock();
  void blitOn(VideoSurface *target, int x, int y);
};


class Video {
  friend VideoSurface;

  int rotation;
  int windowWidth, windowHeight;
  SDL_Window *window;
  SDL_Renderer *renderer;
  VideoSurface *screen;

  VideoSurface* createSurface(int w, int h, bool texture);
public:
  Video(int width, int height, int rotation = 0);
  ~Video();

  inline VideoSurface* getScreen() { return screen; }
  void present();

  inline VideoSurface* createSurface(int w, int h) {
    return createSurface(w, h, false);
  }

  VideoSurface* drawText(TTF_Font *font, const char *str, SDL_Color color);
  inline VideoSurface* adapt(SDL_Surface *surface) {
    return surface ? new VideoSurface(this, surface, nullptr, surface->w, surface->h) : nullptr;
  }
};

#define NUM_SCANCODES (static_cast<int>(SDL_NUM_SCANCODES))

#else
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

class Video;

class VideoSurface {
  friend Video;
  SDL_Surface *surface;
protected:
  VideoSurface(SDL_Surface *surface);
public:
  ~VideoSurface();

  inline int getWidth() { return surface->w; }
  inline int getHeight() { return surface->h; }

  void fill(uint32_t color);
  void fill(int x, int y, int w, int h, uint32_t color);
  int lock(LockedSurface *locked);
  void unlock();
  void blitOn(VideoSurface *target, int x, int y);
};

class Video {
  VideoSurface *screen;
public:
  Video(int width, int height);
  ~Video();

  inline VideoSurface* getScreen() { return screen; }
  void present();

  VideoSurface* createSurface(int w, int h);
  VideoSurface* drawText(TTF_Font *font, const char *str, SDL_Color color);
  inline VideoSurface* adapt(SDL_Surface *surface) {
    return surface ? new VideoSurface(surface) : nullptr;
  }
};

#define NUM_SCANCODES (static_cast<int>(SDLK_LAST))

#endif

int keyCodeFromEvent(const SDL_Event &event);
