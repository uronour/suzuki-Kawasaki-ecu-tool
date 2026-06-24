CC = C:/msys64/mingw64/bin/gcc.exe
CFLAGS = -std=c11 -Wall -Wextra -g -O0 -DSTM32_HAS_FSMC -I../firmware/btt-custom/src/User -I../firmware/btt-custom/src/User/Hal/stm32f2_f4xx -I../firmware/btt-custom/src/User/SD_Logger -I../firmware/btt-custom/src/User/Variants -I../firmware/btt-custom/src/Libraries/cmsis/Core-CM3 -I../firmware/btt-custom/src/Libraries/cmsis/stm32f2xx
LDFLAGS = -lSDL2 -lm

SRC = main.c \
      ../firmware/btt-custom/src/User/gauge_ui.c \
      ../firmware/btt-custom/src/User/gauge_dial.c \
      sim_ili9488_gfx.c \
      ../firmware/btt-custom/src/User/font_8x13.c \
      sdl_gfx.c \
      sdl_input.c \
      sim_data.c \
      stubs.c

OBJ = $(SRC:.c=.o)
TARGET = gauge_sim.exe

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run