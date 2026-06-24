#ifndef _SDL_INPUT_H_
#define _SDL_INPUT_H_

#include <stdint.h>

typedef enum {
    SIM_KEY_NONE = 0,
    SIM_KEY_LEFT,
    SIM_KEY_RIGHT,
    SIM_KEY_UP,
    SIM_KEY_DOWN,
    SIM_KEY_ENTER,
    SIM_KEY_ESCAPE,
    SIM_KEY_QUIT,
    SIM_KEY_D,  // Detail mode toggle
    SIM_KEY_L,  // Log toggle
} SimKey;

void SDL_Input_Init(void);
void SDL_Input_Quit(void);
SimKey SDL_Input_Poll(void);
uint8_t SDL_Input_IsPressed(SimKey key);

#endif