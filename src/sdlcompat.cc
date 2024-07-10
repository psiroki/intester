#include <stdio.h>
#include <stdlib.h>

#include "sdlcompat.hh"

#ifdef USE_SDL2

VideoSurface::VideoSurface(Video *video, SDL_Surface *surface, SDL_Texture *texture, int w, int h):
    video(video), surface(surface), texture(texture), width(w), height(h) {

}

VideoSurface::~VideoSurface() {
  SDL_FreeSurface(surface);
  if (texture) SDL_DestroyTexture(texture);
}

void VideoSurface::update() {
  if (texture) {
    void *ptr = nullptr;
    int bytePitch = 0;
    int result = SDL_LockTexture(texture, nullptr, &ptr, &bytePitch);
    if (result < 0) {
      perror("Fatal: could not lock texture");
      exit(1);
      return;
    }
    uint32_t *dst = static_cast<uint32_t*>(ptr);
    int dp = bytePitch >> 2;
    uint32_t *src = static_cast<uint32_t*>(surface->pixels);
    int sp = surface->pitch >> 2;

    for (int y = 0; y < height; ++y) {
      memcpy(dst + y * dp, src + y * sp, width * 4);
    }
    SDL_UnlockTexture(texture);
  }
}

void VideoSurface::fill(uint32_t color) {
  SDL_FillRect(surface, nullptr, color);
}

void VideoSurface::fill(int x, int y, int w, int h, uint32_t color) {
  if (w <= 0 || h <= 0)
    return;
  SDL_Rect r {
    .x = static_cast<Sint16>(x),
    .y = static_cast<Sint16>(y),
    .w = static_cast<Uint16>(w),
    .h = static_cast<Uint16>(h),
  };
  SDL_FillRect(surface, &r, color);
}

void VideoSurface::unlock() {
  SDL_UnlockSurface(surface);
}

void VideoSurface::blitOn(VideoSurface *target, int x, int y) {
  SDL_Rect rect {
    .x = x,
    .y = y,
  };
  SDL_BlitSurface(surface, nullptr, target->surface, &rect);
}

int VideoSurface::lock(LockedSurface *locked) {
  locked->pixels = nullptr;
  locked->w = width;
  locked->h = height;
  int result = SDL_LockSurface(surface);
  if (result < 0) return result;
  locked->pixels = static_cast<uint8_t*>(surface->pixels);
  locked->pitch = surface->pitch;
  return 0;
}

Video::Video(int width, int height, int rotation): rotation(rotation) {
  windowWidth = (rotation & 1) ? height : width;
  windowHeight = (rotation & 1) ? width : height;
  window = SDL_CreateWindow("Cornellbox", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    windowWidth, windowHeight, SDL_WINDOW_SHOWN);
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  screen = createSurface(width, height, true);
  SDL_SetTextureBlendMode(screen->texture, SDL_BLENDMODE_NONE);
}

Video::~Video() {
  delete screen;
  screen = nullptr;
}

VideoSurface* Video::createSurface(int width, int height, bool scr) {
  SDL_Surface *surface;
  SDL_Texture *texture = nullptr;
  if (scr) {
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 640, 480);
  }
  surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
  return new VideoSurface(this, surface, texture, width, height);
}

VideoSurface* Video::drawText(TTF_Font *font, const char *str, SDL_Color color) {
  SDL_Surface *textSurface = TTF_RenderText_Blended(font, str, color);
  int w = textSurface->w;
  int h = textSurface->h;
  return new VideoSurface(this, textSurface, nullptr, w, h);
}

void Video::present() {
  screen->update();
  SDL_RenderClear(renderer);
  if (rotation) {
    SDL_Rect dst {
      .x = (windowWidth - screen->width) >> 1,
      .y = (windowHeight - screen->height) >> 1,
      .w = screen->width,
      .h = screen->height,
    };
    SDL_RenderCopyEx(renderer, screen->texture, nullptr, &dst, rotation * 90.0, nullptr, SDL_FLIP_NONE);
  } else {
    SDL_RenderCopy(renderer, screen->texture, nullptr, nullptr);
  }
  SDL_RenderPresent(renderer);
}

int keyCodeFromEvent(const SDL_Event &event) {
  return static_cast<int>(event.key.keysym.scancode);
}

#else

VideoSurface::VideoSurface(SDL_Surface *surface): surface(surface) {

}

void VideoSurface::fill(int x, int y, int w, int h, uint32_t color) {
  if (w <= 0 || h <= 0)
    return;
  SDL_Rect r {
    .x = static_cast<Sint16>(x),
    .y = static_cast<Sint16>(y),
    .w = static_cast<Uint16>(w),
    .h = static_cast<Uint16>(h),
  };
  SDL_FillRect(surface, &r, color);
}

void VideoSurface::fill(uint32_t color) {
  SDL_FillRect(surface, nullptr, color);
}

int VideoSurface::lock(LockedSurface *locked) {
  int result = SDL_LockSurface(surface);
  if (result)
    return result;
  locked->w = surface->w;
  locked->h = surface->h;
  locked->pitch = surface->pitch;
  locked->pixels = static_cast<uint8_t*>(surface->pixels);
  return 0;
}

void VideoSurface::unlock() {
  SDL_UnlockSurface(surface);
}

void VideoSurface::blitOn(VideoSurface *target, int x, int y) {
  SDL_Rect rect {
    .x = static_cast<Sint16>(x),
    .y = static_cast<Sint16>(y),
  };
  SDL_BlitSurface(surface, nullptr, target->surface, &rect);
}

VideoSurface::~VideoSurface() {
  SDL_FreeSurface(surface);
}

Video::Video(int w, int h) {
  screen = new VideoSurface(SDL_SetVideoMode(w, h, 32, 0));
}

Video::~Video() {
  delete screen;
  screen = nullptr;
}

VideoSurface* Video::createSurface(int w, int h) {
  return new VideoSurface(SDL_CreateRGBSurface(0, w, h, 32,
      screen->surface->format->Rmask,
      screen->surface->format->Gmask,
      screen->surface->format->Bmask,
      screen->surface->format->Amask));
}

VideoSurface* Video::drawText(TTF_Font *font, const char *str, SDL_Color color) {
  return new VideoSurface(TTF_RenderText_Blended(font, str, color));
}

void Video::present() {
  SDL_Flip(screen->surface);
}

int keyCodeFromEvent(const SDL_Event &event) {
  return static_cast<int>(event.key.keysym.sym);
}

#endif


