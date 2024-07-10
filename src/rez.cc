#include "rez.hh"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <unistd.h>

bool tryGetResolution(Resolution *target) {
  int fb_fd = open("/dev/fb0", O_RDONLY);
  if (fb_fd < 0) {
    return false;
  }

  fb_var_screeninfo vinfo;

  if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo)) {
    perror("Error reading variable information");
    close(fb_fd);
    return false;
  }
  close(fb_fd);

  target->width = vinfo.xres;
  target->height = vinfo.yres;
  return true;
}
