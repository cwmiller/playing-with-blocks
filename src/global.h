#ifndef GLOBAL_H
#define GLOBAL_H

#include "pd_api.h"

// Use 50 FPS as our goal, which is the max the Playdate can do
#define FPS 50

// Global variable to expose Playdate API
extern PlaydateAPI* pd;

// Short-hands for sub-systems exposed by Playdate API
#define GFX pd->graphics
#define SND pd->sound
#define SPR pd->sprite
#define SYS pd->system

#endif