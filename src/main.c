#include <stdio.h>
#include <stdlib.h>

#include "pd_api.h"
#include "game.h"

// Required for windows builds to run within the Simulator
#ifdef _WINDLL
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI* pd, PDSystemEvent event, uint32_t arg) {
	(void)arg;

	// For C API the only thing we need to listen for is initialization.
	// Calling setUpdateCallback disconnects the LUA engine and gives us our update function full control
	if (event == kEventInit) {
		// gameInit is responsible for setting up the game loop
		gameInit(pd);
	}
	
	return 0;
}