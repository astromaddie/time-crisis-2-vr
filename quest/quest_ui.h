#pragma once
#include "vr_math.h"
bool qui_init(void);
#ifdef TCVR_STEAM_FRAME
void qui_draw(const float view[16],const float projection[16],V3 head,Q4 rotation,bool laser,bool physical_crouch,bool left_handed,bool frame_profile_active,int gun_pitch,bool saved);
#else
void qui_draw(const float view[16],const float projection[16],V3 head,Q4 rotation,bool laser,bool physical_crouch,bool left_handed,int gun_pitch,bool saved);
#endif
void qui_shutdown(void);
