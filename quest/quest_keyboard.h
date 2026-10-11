#pragma once
#include <stdbool.h>
#include <SDL2/SDL.h>
/* Platform request seam; Frame's SteamVR bridge requires runtime evidence. */
void qkeyboard_update(void);
bool qkeyboard_event(const SDL_Event *event);
