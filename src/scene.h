#ifndef SCENES_H
#define SCENES_H

#include "pd_api.h"
#include <stdbool.h>

typedef struct Scene {
    // Name of scene
    const char* name;

    // Called on first frame of scene transition after previous scene is destroyed. 
    // Called within the same frame as the previous scene's destruction handler
    // Always triggers a screen redraw.
    void (*init)(struct Scene* scene);

    // Called on each frame of active scene. Return value is whether or not the screen should redraw.
    bool (*update)(struct Scene* scene);

    // Called on last frame when scene is being transitioned away. 
    // Always triggers a screen redraw after next scene takes over.
    void (*destroy)(struct Scene* scene);

    // Scene state
    void* data;
} Scene;

#endif