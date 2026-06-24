#include <stdio.h>
#include <windows.h>
#include <SDL2/SDL.h>
#include "sdl_gfx.h"
#include "sdl_input.h"
#include "sim_data.h"
#include "gauge_ui.h"
#include "gauge_dial.h"
#include "os_timer.h"

int main(int argc, char *argv[]);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    return main(__argc, __argv);
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    printf("GSX-R1000 Gauge Simulator starting...\n");
    printf("Controls:\n");
    printf("  LEFT/RIGHT  - Change page\n");
    printf("  UP/DOWN     - Shift gear up/down\n");
    printf("  SPACE       - Hold throttle\n");
    printf("  C           - Toggle clutch\n");
    printf("  D           - Toggle detail mode (dashboard page)\n");
    printf("  L           - Toggle logging (settings page)\n");
    printf("  ESC/Q       - Quit\n\n");

    SDL_GFX_Init();
    SDL_Input_Init();
    SimData_Init();

    // Initialize gauge UI
    DIAL_InitTable();
    Gauge_Init();

    uint8_t running = 1;
    uint8_t throttleHeld = 0;

    while (running) {
        SimKey key = SDL_Input_Poll();

        if (key == SIM_KEY_QUIT || key == SIM_KEY_ESCAPE) {
            running = 0;
        }

        if (SDL_Input_IsPressed(SIM_KEY_LEFT)) {
            Gauge_PrevPage();
        }
        if (SDL_Input_IsPressed(SIM_KEY_RIGHT)) {
            Gauge_NextPage();
        }
        if (SDL_Input_IsPressed(SIM_KEY_UP)) {
            SimData_ShiftUp();
        }
        if (SDL_Input_IsPressed(SIM_KEY_DOWN)) {
            SimData_ShiftDown();
        }
        if (SDL_Input_IsPressed(SIM_KEY_D)) {
            Gauge_Press();  // Toggle detail mode on dashboard
        }
        if (SDL_Input_IsPressed(SIM_KEY_L)) {
            Gauge_Press();  // Toggle logging on settings page
        }

        // Throttle handling (space key)
        const uint8_t *keystate = SDL_GetKeyboardState(NULL);
        throttleHeld = keystate[SDL_SCANCODE_SPACE];
        SimData_SetThrottle(throttleHeld);

        if (keystate[SDL_SCANCODE_C]) {
            SimData_ToggleClutch();
            SDL_Delay(200);  // Debounce
        }

        // Update simulation
        SimData_Update();

        // Update gauge UI
        Gauge_Update();

        // Render
        SDL_GFX_Present();

        // Frame rate limiting
        SDL_Delay(16);
    }

    printf("Shutting down...\n");
    SDL_Input_Quit();
    SDL_GFX_Quit();

    return 0;
}