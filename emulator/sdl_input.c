#include "sdl_input.h"
#include <SDL2/SDL.h>

static uint8_t g_keyStates[16] = {0};
static uint8_t g_prevKeyStates[16] = {0};

static SimKey sdl_key_to_sim(SDL_Keycode key)
{
    switch (key) {
        case SDLK_LEFT: return SIM_KEY_LEFT;
        case SDLK_RIGHT: return SIM_KEY_RIGHT;
        case SDLK_UP: return SIM_KEY_UP;
        case SDLK_DOWN: return SIM_KEY_DOWN;
        case SDLK_RETURN: return SIM_KEY_ENTER;
        case SDLK_ESCAPE: return SIM_KEY_ESCAPE;
        case SDLK_d: return SIM_KEY_D;
        case SDLK_l: return SIM_KEY_L;
        case SDLK_q: return SIM_KEY_QUIT;
        default: return SIM_KEY_NONE;
    }
}

void SDL_Input_Init(void)
{
}

void SDL_Input_Quit(void)
{
}

SimKey SDL_Input_Poll(void)
{
    memcpy(g_prevKeyStates, g_keyStates, sizeof(g_keyStates));

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            g_keyStates[SIM_KEY_QUIT] = 1;
            return SIM_KEY_QUIT;
        }
        else if (event.type == SDL_KEYDOWN) {
            SimKey k = sdl_key_to_sim(event.key.keysym.sym);
            if (k != SIM_KEY_NONE) g_keyStates[k] = 1;
        }
        else if (event.type == SDL_KEYUP) {
            SimKey k = sdl_key_to_sim(event.key.keysym.sym);
            if (k != SIM_KEY_NONE) g_keyStates[k] = 0;
        }
    }
    return SIM_KEY_NONE;
}

uint8_t SDL_Input_IsPressed(SimKey key)
{
    if (key >= 16) return 0;
    return g_keyStates[key] && !g_prevKeyStates[key];
}