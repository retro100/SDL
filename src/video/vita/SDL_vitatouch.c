/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2017 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_VITA

#include <psp2/kernel/processmgr.h>
#include <psp2/touch.h>

#include "SDL_events.h"
#include "SDL_log.h"
#include "SDL_vitavideo.h"
#include "SDL_vitatouch.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_touch_c.h"

static SceTouchData touch_old[SCE_TOUCH_PORT_MAX_NUM];
static SceTouchData touch[SCE_TOUCH_PORT_MAX_NUM];
static SceTouchPanelInfo panelinfo[SCE_TOUCH_PORT_MAX_NUM];

static float aAWidth[SCE_TOUCH_PORT_MAX_NUM];
static float aAHeight[SCE_TOUCH_PORT_MAX_NUM];
static float dispWidth[SCE_TOUCH_PORT_MAX_NUM];
static float dispHeight[SCE_TOUCH_PORT_MAX_NUM];
static float forcerange[SCE_TOUCH_PORT_MAX_NUM];

void 
VITA_InitTouch(void)
{
	sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
	sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_START);
	sceTouchEnableTouchForce(SCE_TOUCH_PORT_FRONT);
	sceTouchEnableTouchForce(SCE_TOUCH_PORT_BACK);

	for(int port = 0; port < SCE_TOUCH_PORT_MAX_NUM; port++) {
		sceTouchGetPanelInfo(port, &panelinfo[port]);
		aAWidth[port] = (float)(panelinfo[port].maxAaX - panelinfo[port].minAaX);
		aAHeight[port] = (float)(panelinfo[port].maxAaY - panelinfo[port].minAaY);
		dispWidth[port] = (float)(panelinfo[port].maxDispX - panelinfo[port].minDispX);
		dispHeight[port] = (float)(panelinfo[port].maxDispY - panelinfo[port].minDispY);
		forcerange[port] = (float)(panelinfo[port].maxForce - panelinfo[port].minForce);
	}

	// Support passing both front and back touch devices in events
	SDL_AddTouch((SDL_TouchID)0, "Front");
	SDL_AddTouch((SDL_TouchID)1, "Back");
}

void 
VITA_QuitTouch(void){
	sceTouchDisableTouchForce(SCE_TOUCH_PORT_FRONT);
	sceTouchDisableTouchForce(SCE_TOUCH_PORT_BACK);
}

void 
VITA_PollTouch(void)
{
	SDL_FingerID finger_id = 0;
	float force;
	int port;
	
	// We skip polling touch if no window is created
	if (Vita_Window == NULL)
		return;

	memcpy(touch_old, touch, sizeof(touch_old));

	for(port = 0; port < SCE_TOUCH_PORT_MAX_NUM; port++) {
		sceTouchPeek(port, &touch[port], 1);
		if (touch[port].reportNum > 0) {
			for (int i = 0; i < touch[port].reportNum; i++)
			{
				// adjust coordinates and forces to return normalized values
				// for the front, screen area is used as a reference (for direct touch)
				// e.g. touch_x = 1.0 corresponds to screen_x = 960
				// for the back panel, the active touch area is used as reference
				float x = 0;
				float y = 0;
				VITA_ConvertTouchXYToSDLXY(&x, &y, touch[port].report[i].x, touch[port].report[i].y, port);
				force = (touch[port].report[i].force - panelinfo[port].minForce) / forcerange[port];
				finger_id = (SDL_FingerID) touch[port].report[i].id;

				// Send an initial touch
				SDL_SendTouch((SDL_TouchID)port,
					finger_id,
					SDL_TRUE,
					x,
					y,
					force);

				// Always send the motion
				SDL_SendTouchMotion((SDL_TouchID)port,
					finger_id,
					x,
					y,
					force);
			}
		}

		// some fingers might have been let go
		if (touch_old[port].reportNum > 0) {
			for (int i = 0; i < touch_old[port].reportNum; i++) {
				int finger_up = 1;
				if (touch[port].reportNum > 0) {
					for (int j = 0; j < touch[port].reportNum; j++) {
						if (touch[port].report[j].id == touch_old[port].report[i].id ) {
							finger_up = 0;
						}
					}
				}
				if (finger_up == 1) {
					float x = 0;
					float y = 0;
					VITA_ConvertTouchXYToSDLXY(&x, &y, touch_old[port].report[i].x, touch_old[port].report[i].y, port);
					force = (touch_old[port].report[i].force - panelinfo[port].minForce) / forcerange[port];
					finger_id = (SDL_FingerID) touch_old[port].report[i].id;
					// Finger released from screen
					SDL_SendTouch((SDL_TouchID)port,
						finger_id,
						SDL_FALSE,
						x,
						y,
						force);
				}
			}
		}
	}
}

void VITA_ConvertTouchXYToSDLXY(float *sdl_x, float *sdl_y, int vita_x, int vita_y, int port) {
	float x = 0;
	float y = 0;
	if (port == SCE_TOUCH_PORT_FRONT) {
		x = (vita_x - panelinfo[port].minDispX) / dispWidth[port];
		y = (vita_y - panelinfo[port].minDispY) / dispHeight[port];
	} else {
		x = (vita_x - panelinfo[port].minAaX) / aAWidth[port];
		y = (vita_y - panelinfo[port].minAaY) / aAHeight[port];				
	}
	if (x < 0.0) {
		x = 0.0;
	} else if (x > 1.0) {
		x = 1.0;
	}
	if (y < 0.0) {
		y = 0.0;
	} else if (y > 1.0) {
		y = 1.0;
	}
	*sdl_x = x;
	*sdl_y = y;
}


#endif /* SDL_VIDEO_DRIVER_VITA */

/* vi: set ts=4 sw=4 expandtab: */
