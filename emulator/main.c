#include <stdio.h>
#include <windows.h>
#include <SDL2/SDL.h>
#include "sdl_gfx.h"
#include "sdl_input.h"
#include "sim_data.h"
#include "gauge_ui.h"
#include "gauge_dial.h"
#include "os_timer.h"
#include "../firmware/btt-custom/src/User/boot_screen.h"

extern OS_COUNTER os_counter;

int main(int argc, char *argv[]);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    return main(__argc, __argv);
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    printf("=== GSX-R1000 Gauge Simulator Starting ===\n");
    printf("Controls:\n");
    printf("  LEFT/RIGHT  - Change page\n");
    printf("  UP/DOWN     - Shift gear up/down\n");
    printf("  SPACE       - Hold throttle\n");
    printf("  C           - Toggle clutch\n");
    printf("  D           - Toggle detail mode (dashboard page)\n");
    printf("  L           - Toggle logging (settings page)\n");
    printf("  ESC/Q       - Quit\n\n");

    printf("[DEBUG] Initializing SDL_GFX...\n");
    SDL_GFX_Init();
    printf("[DEBUG] SDL_GFX initialized, window created\n");
    
    printf("[DEBUG] Initializing SDL_Input...\n");
    SDL_Input_Init();
    
    printf("[DEBUG] Initializing SimData...\n");
    SimData_Init();
    printf("[DEBUG] SimData initialized\n");

    // Initialize gauge UI
    printf("[DEBUG] Initializing DIAL table...\n");
    DIAL_InitTable();
    printf("[DEBUG] DIAL table initialized\n");
    
    // Run boot animation
    printf("[DEBUG] Running boot animation...\n");
    Boot_Animation();
    printf("[DEBUG] Boot animation complete\n");
    
    printf("[DEBUG] Initializing Gauge UI...\n");
    Gauge_Init();
    printf("[DEBUG] Gauge UI initialized\n");

    // TEST MODE: Run for 10 seconds then exit
    uint32_t testStartTime = 0;
    uint8_t testMode = 1;

    uint8_t running = 1;
    uint8_t throttleHeld = 0;
    uint32_t frameCount = 0;

    while (running) {
        SimKey key = SDL_Input_Poll();

        if (key == SIM_KEY_QUIT || key == SIM_KEY_ESCAPE) {
            running = 0;
        }

        if (SDL_Input_IsPressed(SIM_KEY_LEFT)) {
            printf("[DEBUG] LEFT pressed - Prev page\n");
            Gauge_PrevPage();
        }
        if (SDL_Input_IsPressed(SIM_KEY_RIGHT)) {
            printf("[DEBUG] RIGHT pressed - Next page\n");
            Gauge_NextPage();
        }
        if (SDL_Input_IsPressed(SIM_KEY_UP)) {
            SimData_ShiftUp();
        }
        if (SDL_Input_IsPressed(SIM_KEY_DOWN)) {
            SimData_ShiftDown();
        }
        if (SDL_Input_IsPressed(SIM_KEY_D)) {
            printf("[DEBUG] D pressed - Toggle detail\n");
            Gauge_Press();
        }
        if (SDL_Input_IsPressed(SIM_KEY_L)) {
            Gauge_Press();
        }

        // Throttle handling (space key)
        const uint8_t *keystate = SDL_GetKeyboardState(NULL);
        throttleHeld = keystate[SDL_SCANCODE_SPACE];
        SimData_SetThrottle(throttleHeld);

        if (keystate[SDL_SCANCODE_C]) {
            SimData_ToggleClutch();
            SDL_Delay(200);
        }

        // Update simulation
        SimData_Update();

        // Update gauge UI
        Gauge_Update();

        // Render
        SDL_GFX_Present();

        frameCount++;
        if (frameCount % 60 == 0) {
            printf("[DEBUG] Frame %u, RPM=%u, Gear=%u, Page=%d\n", 
                   frameCount, g_sdsData.rpm, g_sdsData.gearPos, Gauge_GetPage());
        }

        // Update firmware timer for boot animation etc.
        os_counter.ms = SDL_GetTicks() - SDL_GFX_GetStartTime();

        // Frame rate limiting
        SDL_Delay(16);

        // Test mode: exit after 30 seconds
        if (testMode) {
            if (testStartTime == 0) testStartTime = SDL_GetTicks();
            if (SDL_GetTicks() - testStartTime > 30000) {
                printf("[TEST] 30 seconds elapsed, exiting...\n");
                running = 0;
            }
        }
    }

    printf("\n[DEBUG] Shutting down... (frames: %u)\n", frameCount);
    SDL_Input_Quit();
    SDL_GFX_Quit();

    return 0;
}

