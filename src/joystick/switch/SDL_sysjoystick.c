/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_JOYSTICK_SWITCH

/* This is the dummy implementation of the SDL joystick API */

#include "SDL_events.h"
#include "../SDL_sysjoystick.h"

#include <switch.h>

#define JOYSTICK_COUNT 8

typedef struct SWITCHJoystickState
{
    PadState pad;
    u64 buttons;
    HidAnalogStickState stick_l;
    HidAnalogStickState stick_r;
    HidVibrationDeviceHandle vibrationDeviceHandles;
    HidVibrationValue vibrationValues;
} SWITCHJoystickState;

static SWITCHJoystickState joystickState[JOYSTICK_COUNT];

static const HidNpadButton pad_mapping[] = {
        HidNpadButton_A, HidNpadButton_B, HidNpadButton_X, HidNpadButton_Y,
        HidNpadButton_StickL, HidNpadButton_StickR,
        HidNpadButton_L, HidNpadButton_R,
        HidNpadButton_ZL, HidNpadButton_ZR,
        HidNpadButton_Plus, HidNpadButton_Minus,
        HidNpadButton_Left, HidNpadButton_Up, HidNpadButton_Right, HidNpadButton_Down,
        HidNpadButton_StickLLeft, HidNpadButton_StickLUp, HidNpadButton_StickLRight, HidNpadButton_StickLDown,
        HidNpadButton_StickRLeft, HidNpadButton_StickRUp, HidNpadButton_StickRRight, HidNpadButton_StickRDown,
        HidNpadButton_LeftSL, HidNpadButton_LeftSR, HidNpadButton_RightSL, HidNpadButton_RightSR
};

/* Function to scan the system for joysticks.
 * It should return 0, or -1 on an unrecoverable fatal error.
 */
static int
SWITCH_JoystickInit(void)
{
    padConfigureInput(JOYSTICK_COUNT, HidNpadStyleSet_NpadStandard);

    // initialize first pad to defaults
    padInitializeDefault(&joystickState[0].pad);
    padUpdate(&joystickState[0].pad);

    // initialize pad and vibrations for pad 1 to 7
    for (int i = 1; i < JOYSTICK_COUNT; i++) {
        padInitialize(&joystickState[i].pad, HidNpadIdType_No1 + i);
        padUpdate(&joystickState[i].pad);
        // we have two or more joycons detected in dual mode, assume we want single joycon mode
        if(padGetStyleSet(&joystickState[i].pad) & HidNpadStyleTag_NpadJoyDual) {
            hidSetNpadJoyAssignmentModeSingleByDefault(HidNpadIdType_No1);
            hidSetNpadJoyAssignmentModeSingleByDefault(HidNpadIdType_No1 + i);
            hidSetNpadJoyHoldType(HidNpadJoyHoldType_Horizontal);
        }
        hidInitializeVibrationDevices(&joystickState[i].vibrationDeviceHandles,1,
                                      HidNpadIdType_No1 + i, padGetStyleSet(&joystickState[i].pad));
    }

    // finally, initialize first pad vibrations, style may have changed since pad init
    hidInitializeVibrationDevices(&joystickState[0].vibrationDeviceHandles,1,
                                  padIsHandheld(&joystickState[0].pad) ? HidNpadIdType_Handheld : HidNpadIdType_No1,
                                  padGetStyleSet(&joystickState[0].pad));

    return JOYSTICK_COUNT;
}

static int
SWITCH_JoystickGetCount(void)
{
    return JOYSTICK_COUNT;
}

static void
SWITCH_JoystickDetect(void)
{
}

/* Function to get the device-dependent name of a joystick */
static const char *
SWITCH_JoystickGetDeviceName(int device_index)
{
    return "Switch Controller";
}

static int
SWITCH_JoystickGetDevicePlayerIndex(int device_index)
{
    return -1;
}

static void
SWITCH_JoystickSetDevicePlayerIndex(int device_index, int player_index)
{
}

static SDL_JoystickGUID
SWITCH_JoystickGetDeviceGUID(int device_index)
{
    SDL_JoystickGUID guid;
    /* the GUID is just the first 16 chars of the name for now */
    const char *name = SWITCH_JoystickGetDeviceName(device_index);
    SDL_zero(guid);
    SDL_memcpy(&guid, name, SDL_min(sizeof(guid), SDL_strlen(name)));
    return guid;
}

/* Function to perform the mapping from device index to the instance id for this index */
static SDL_JoystickID
SWITCH_JoystickGetDeviceInstanceID(int device_index)
{
    return device_index;
}

/* Function to open a joystick for use.
   The joystick to open is specified by the device index.
   This should fill the nbuttons and naxes fields of the joystick structure.
   It returns 0, or -1 if there is an error.
 */
static int
SWITCH_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
    joystick->nbuttons = sizeof(pad_mapping) / sizeof(*pad_mapping);
    joystick->naxes = 4;
    joystick->nhats = 0;
    joystick->instance_id = device_index;

    return 0;
}

static int
SWITCH_JoystickRumble(SDL_Joystick * joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    int id = joystick->instance_id;

    joystickState[id].vibrationValues.amp_low =
            joystickState[id].vibrationValues.amp_high = low_frequency_rumble == 0 ? 0.0f : 320.0f;
    joystickState[id].vibrationValues.freq_low =
            low_frequency_rumble == 0 ? 160.0f : (float) low_frequency_rumble / 204;
    joystickState[id].vibrationValues.freq_high =
            high_frequency_rumble == 0 ? 320.0f : (float) high_frequency_rumble / 204;

    hidSendVibrationValues(&joystickState[id].vibrationDeviceHandles, &joystickState[id].vibrationValues, 1);

    return 0;
}

/* Function to update the state of a joystick - called as a device poll.
 * This function shouldn't update the joystick structure directly,
 * but instead should call SDL_PrivateJoystick*() to deliver events
 * and update joystick device state.
 */
static void
SWITCH_JoystickUpdate(SDL_Joystick *joystick)
{
    u64 changed;
    HidNpadStyleTag style;
    static SWITCHJoystickState state_old[JOYSTICK_COUNT];

    int index = (int) SDL_JoystickInstanceID(joystick);
    if (index > JOYSTICK_COUNT || SDL_IsTextInputActive()) {
        return;
    }

    padUpdate(&joystickState[index].pad);
    joystickState[index].stick_l = padGetStickPos(&joystickState[index].pad, 0);
    joystickState[index].stick_r = padGetStickPos(&joystickState[index].pad, 1);
    joystickState[index].buttons = padGetButtons(&joystickState[index].pad);

    style = padGetStyleSet(&joystickState[index].pad);
    // we have two or more pads detected in dual mode, assume we want single joycon mode
    if(style & HidNpadStyleTag_NpadJoyDual) {
        hidSetNpadJoyAssignmentModeSingleByDefault(HidNpadIdType_No1);
        hidSetNpadJoyAssignmentModeSingleByDefault(HidNpadIdType_No1 + index);
        hidSetNpadJoyHoldType(HidNpadJoyHoldType_Horizontal);
    }
    if(style & HidNpadStyleTag_NpadJoyLeft) {
        if(joystickState[index].buttons & HidNpadButton_Left) {
            joystickState[index].buttons &= ~HidNpadButton_Left;
            joystickState[index].buttons |= HidNpadButton_B;
        } else if(joystickState[index].buttons & HidNpadButton_Down) {
            joystickState[index].buttons &= ~HidNpadButton_Down;
            joystickState[index].buttons |= HidNpadButton_A;
        } else if(joystickState[index].buttons & HidNpadButton_Up) {
            joystickState[index].buttons &= ~HidNpadButton_Up;
            joystickState[index].buttons |= HidNpadButton_Y;
        } else if(joystickState[index].buttons & HidNpadButton_Right) {
            joystickState[index].buttons &= ~HidNpadButton_Right;
            joystickState[index].buttons |= HidNpadButton_X;
        } else if(joystickState[index].buttons & HidNpadButton_LeftSL) {
            joystickState[index].buttons &= ~HidNpadButton_LeftSL;
            joystickState[index].buttons |= HidNpadButton_L;
        } else if(joystickState[index].buttons & HidNpadButton_LeftSR) {
            joystickState[index].buttons &= ~HidNpadButton_LeftSR;
            joystickState[index].buttons |= HidNpadButton_R;
        }
    } else if( style & HidNpadStyleTag_NpadJoyRight) {
        if(joystickState[index].buttons & HidNpadButton_A) {
            joystickState[index].buttons &= ~HidNpadButton_A;
            joystickState[index].buttons |= HidNpadButton_B;
        } else if(joystickState[index].buttons & HidNpadButton_X) {
            joystickState[index].buttons &= ~HidNpadButton_X;
            joystickState[index].buttons |= HidNpadButton_A;
        } else if(joystickState[index].buttons & HidNpadButton_B) {
            joystickState[index].buttons &= ~HidNpadButton_B;
            joystickState[index].buttons |= HidNpadButton_Y;
        } else if(joystickState[index].buttons & HidNpadButton_Y) {
            joystickState[index].buttons &= ~HidNpadButton_Y;
            joystickState[index].buttons |= HidNpadButton_X;
        } else if(joystickState[index].buttons & HidNpadButton_RightSL) {
            joystickState[index].buttons &= ~HidNpadButton_RightSL;
            joystickState[index].buttons |= HidNpadButton_L;
        } else if(joystickState[index].buttons & HidNpadButton_RightSR) {
            joystickState[index].buttons &= ~HidNpadButton_RightSR;
            joystickState[index].buttons |= HidNpadButton_R;
        }
    }

    // Axes
    if (state_old[index].stick_l.x != joystickState[index].stick_l.x) {
        SDL_PrivateJoystickAxis(joystick, 0, (Sint16) joystickState[index].stick_l.x);
        state_old[index].stick_l.x = joystickState[index].stick_l.x;
    }
    if (state_old[index].stick_l.y != joystickState[index].stick_l.y) {
        SDL_PrivateJoystickAxis(joystick, 1, (Sint16) -joystickState[index].stick_l.y);
        state_old[index].stick_l.y = -joystickState[index].stick_l.y;
    }
    if (state_old[index].stick_r.x != joystickState[index].stick_r.x) {
        SDL_PrivateJoystickAxis(joystick, 2, (Sint16) joystickState[index].stick_r.x);
        state_old[index].stick_r.x = joystickState[index].stick_r.x;
    }
    if (state_old[index].stick_r.y != joystickState[index].stick_r.y) {
        SDL_PrivateJoystickAxis(joystick, 3, (Sint16) -joystickState[index].stick_r.y);
        state_old[index].stick_r.y = -joystickState[index].stick_r.y;
    }

    // Buttons
    changed = state_old[index].buttons ^ joystickState[index].buttons;
    state_old[index].buttons = joystickState[index].buttons;
    if (changed) {
        for (int i = 0; i < joystick->nbuttons; i++) {
            if (changed & pad_mapping[i]) {
                SDL_PrivateJoystickButton(joystick, (Uint8) i,
                        (Uint8) ((joystickState[index].buttons & pad_mapping[i]) ?
                        SDL_PRESSED : SDL_RELEASED));
            }
        }
    }
}

/* Function to close a joystick after use */
static void
SWITCH_JoystickClose(SDL_Joystick *joystick)
{
}

/* Function to perform any system-specific joystick related cleanup */
static void
SWITCH_JoystickQuit(void)
{
}

SDL_JoystickDriver SDL_SWITCH_JoystickDriver =
{
    SWITCH_JoystickInit,
    SWITCH_JoystickGetCount,
    SWITCH_JoystickDetect,
    SWITCH_JoystickGetDeviceName,
    SWITCH_JoystickGetDevicePlayerIndex,
    SWITCH_JoystickSetDevicePlayerIndex,
    SWITCH_JoystickGetDeviceGUID,
    SWITCH_JoystickGetDeviceInstanceID,
    SWITCH_JoystickOpen,
    SWITCH_JoystickRumble,
    SWITCH_JoystickUpdate,
    SWITCH_JoystickClose,
    SWITCH_JoystickQuit,
};

#endif /* SDL_JOYSTICK_SWITCH */

/* vi: set ts=4 sw=4 expandtab: */