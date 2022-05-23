#ifndef GLOBAL_H
#define GLOBAL_H

#include "pd_api.h"

// Use 60 FPS as our goal
#define FPS 60

// Global variable to expose Playdate API
extern PlaydateAPI* pd;

// Short-hands for sub-systems exposed by Playdate API
#define GFX pd->graphics
#define SPR pd->sprite
#define SYS pd->system

#endif