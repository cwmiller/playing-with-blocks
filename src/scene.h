#ifndef SCENES_H
#define SCENES_H

#include "pd_api.h"
#include "scene.h"

typedef struct Scene {
    const char* name;
    void (*init)(struct Scene* scene);
    void (*update)(struct Scene* scene);
    void (*destroy)(struct Scene* scene);
    void* data;
} Scene;

#endif