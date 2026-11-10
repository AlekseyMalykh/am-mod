#ifndef AM_SCREEN_MIRROR_FRAME_H
#define AM_SCREEN_MIRROR_FRAME_H

#define MIRROR_WIDTH 320u
#define MIRROR_HEIGHT 240u
#define MIRROR_BMP_BYTES (54u+MIRROR_WIDTH*MIRROR_HEIGHT*3u)
#ifndef SCREEN_MIRROR_FLAG
#define SCREEN_MIRROR_FLAG "/run/mpc-trial-app/screen-mirror.on"
#endif
#ifndef SCREEN_MIRROR_FRAME
#define SCREEN_MIRROR_FRAME "/run/mpc-trial-app/screen-mirror.bmp"
#endif

#endif
