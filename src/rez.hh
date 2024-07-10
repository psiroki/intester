#pragma once

struct Resolution {
  int width;
  int height;
};

bool tryGetResolution(Resolution *target);
