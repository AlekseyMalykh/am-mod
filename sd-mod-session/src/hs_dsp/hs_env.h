#ifndef HS_ENV_H
#define HS_ENV_H
/* The HS key bus: a Pump or Crush sends its input level on bus A-D, and a Pump or Crush
   on another track keys its compressor from that bus (sidechain ducking). Every HS instance lives
   in this one library, so a static array is the whole bus. */
enum { HS_ENV_BUSES = 4 };
void hs_env_send(int bus, float peak);   /* bus 1-4: linear peak of one block */
float hs_env_read(int bus);              /* 0 once the bus has had no send for 100 ms */
#endif
