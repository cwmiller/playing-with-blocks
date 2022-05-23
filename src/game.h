#ifndef GAME_H
#define GAME_H

#include "scene.h"

// Initializes game state and loop
void gameInit(PlaydateAPI* pd);

// Transition to a new scene. Current scene will be terminated and the new one will be displayed on next frame.
void gameChangeScene(Scene* scene);

#endif