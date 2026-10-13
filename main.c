#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include <mmsystem.h> 

#define MEMORY_SIZE 4096
#define SCREEN_W 64
#define SCREEN_H 32
#define ROM_START 0x200
#define MAX_ROM_SIZE (MEMORY_SIZE - ROM_START)
#define FONT_START 0x050
#define CYCLES_PER_FRAME 15

static double now_sec(void) {
    LARGE_INTEGER f, t;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)f.QuadPart;
}

static const uint8_t font[80] = {
    0xF0,0x90,0x90,0x90,0xF0, // 0
    0x20,0x60,0x20,0x20,0x70, // 1
    0xF0,0x10,0xF0,0x80,0xF0, // 2
    0xF0,0x10,0xF0,0x10,0xF0, // 3
    0x90,0x90,0xF0,0x10,0x10, // 4
    0xF0,0x80,0xF0,0x10,0xF0, // 5
    0xF0,0x80,0xF0,0x90,0xF0, // 6
    0xF0,0x10,0x20,0x40,0x40, // 7
    0xF0,0x90,0xF0,0x90,0xF0, // 8
    0xF0,0x90,0xF0,0x10,0xF0, // 9
    0xF0,0x90,0xF0,0x90,0x90, // A
    0xE0,0x90,0xE0,0x90,0xE0, // B
    0xF0,0x80,0x80,0x80,0xF0, // C
    0xE0,0x90,0x90,0x90,0xE0, // D
    0xF0,0x80,0xF0,0x80,0xF0, // E
    0xF0,0x80,0xF0,0x80,0x80  // F
};

static const int keymap[16] = {
    'X', '1', '2', '3',   // 0 1 2 3
    'Q', 'W', 'E', 'A',   // 4 5 6 7
    'S', 'D', 'Z', 'C',   // 8 9 A B
    '4', 'R', 'F', 'V'    // C D E F
};

static void report_unimplemented(uint16_t opcode, uint16_t pc) {
    fprintf(stderr, "unimplemented opcode 0x%04X at 0x%03X\n", opcode, pc - 2);
}

typedef struct {
    uint8_t memory[MEMORY_SIZE];
    uint8_t v_registers[16];
    uint16_t I;
    uint16_t pc;
    uint16_t stack[16];
    uint8_t sp;
    uint8_t display[(SCREEN_W * SCREEN_H)];
    uint8_t keypad[16];
    uint8_t delay_timer;
    uint8_t sound_timer;
} Chip8;

static void render(const uint8_t *display) {
    static char buf[SCREEN_H * (SCREEN_W * 2 + 1) + 8];
    char *p = buf;
    *p++ = '\033'; *p++ = '['; *p++ = 'H'; 
    for (int row = 0; row < SCREEN_H; row++) {
        for (int col = 0; col < SCREEN_W; col++) {
            char c = display[row * SCREEN_W + col] ? '#' : ' ';
            *p++ = c;                             
            *p++ = c;
        }
        *p++ = '\n';
    }
    fwrite(buf, 1, (size_t)(p - buf), stdout);
    fflush(stdout);
}

int main(int argc, char *argv[]) {

    Chip8 chip = {0};
    chip.pc = ROM_START;

    srand((unsigned)time(NULL));
    memcpy(&chip.memory[FONT_START], font, sizeof(font));

    const char *path = (argc > 1) ? argv[1] : "Paddles.ch8";
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        perror("fopen");
        return 1;
    }

    // ROM size check
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    if (size > MAX_ROM_SIZE || size <= 0) {
        fprintf(stderr, "bad ROM size: %ld bytes (max %d)\n", size, MAX_ROM_SIZE);
        fclose(fp);
        return 1;
    }

    size_t real_rom_size = fread(&chip.memory[ROM_START], 1, sizeof(chip.memory) - ROM_START, fp);
    fclose(fp);
    if ((long)real_rom_size != size) {
        fprintf(stderr, "rom size and bytes read mismatch\n");
        return 1;
    }

    printf("\033[2J"); 
    int needs_redraw = 0;


    timeBeginPeriod(1);
    double next_frame = now_sec();

    while (1) {
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) break;
        for (int k = 0; k < 16; k++) {
            chip.keypad[k] = (GetAsyncKeyState(keymap[k]) & 0x8000) != 0;
        }


        for (int cycle = 0; cycle < CYCLES_PER_FRAME; cycle++) {
            uint16_t opcode = chip.memory[chip.pc] * 256 + chip.memory[chip.pc + 1];
            chip.pc += 2;

            uint8_t x = (opcode & 0x0F00) >> 8;
            uint8_t y = (opcode & 0x00F0) >> 4;
            uint8_t N = (opcode & 0x000F);
            uint8_t NN = (opcode & 0x00FF);
            uint16_t NNN = (opcode & 0x0FFF);

            switch ((opcode & 0xF000) >> 12) {
                case 0x0:
                    switch (NN) {
                        case 0xE0:
                            memset(chip.display, 0, sizeof(chip.display));
                            needs_redraw = 1;
                            break;
                        case 0xEE:
                            if (chip.sp == 0) { fprintf(stderr, "stack underflow\n"); return 1; }
                            chip.pc = chip.stack[--chip.sp];
                            break;
                        default:
                            report_unimplemented(opcode, chip.pc);
                            return 1;
                    }
                    break;
                case 0x1:
                    chip.pc = NNN;
                    break;
                case 0x2:
                    if (chip.sp >= 16) { fprintf(stderr, "stack overflow\n"); return 1; }
                    chip.stack[chip.sp++] = chip.pc;
                    chip.pc = NNN;
                    break;
                case 0x3:
                    if (chip.v_registers[x] == NN) chip.pc += 2;
                    break;
                case 0x4:
                    if (chip.v_registers[x] != NN) chip.pc += 2;
                    break;
                case 0x5:
                    if (chip.v_registers[x] == chip.v_registers[y]) chip.pc += 2;
                    break;
                case 0x6:
                    chip.v_registers[x] = NN;
                    break;
                case 0x7:
                    chip.v_registers[x] += NN;
                    break;
                case 0x8:
                    switch (N) {
                        case 0x0: chip.v_registers[x]  = chip.v_registers[y]; break;
                        case 0x1: chip.v_registers[x] |= chip.v_registers[y]; break;
                        case 0x2: chip.v_registers[x] &= chip.v_registers[y]; break;
                        case 0x3: chip.v_registers[x] ^= chip.v_registers[y]; break;
                        case 0x4: {
                            uint16_t sum = chip.v_registers[x] + chip.v_registers[y];
                            chip.v_registers[x] = sum & 0xFF;
                            chip.v_registers[0xF] = sum > 0xFF;
                            break;
                        }
                        case 0x5: {
                            uint8_t vx = chip.v_registers[x], vy = chip.v_registers[y];
                            chip.v_registers[x] = vx - vy;
                            chip.v_registers[0xF] = vx >= vy;
                            break;
                        }
                        case 0x6: {
                            uint8_t vx = chip.v_registers[x];
                            chip.v_registers[x] = vx >> 1;
                            chip.v_registers[0xF] = vx & 1;
                            break;
                        }
                        case 0x7: {
                            uint8_t vx = chip.v_registers[x], vy = chip.v_registers[y];
                            chip.v_registers[x] = vy - vx;
                            chip.v_registers[0xF] = vy >= vx;
                            break;
                        }
                        case 0xE: {
                            uint8_t vx = chip.v_registers[x];
                            chip.v_registers[x] = vx << 1;
                            chip.v_registers[0xF] = (vx >> 7) & 1;
                            break;
                        }
                        default:
                            report_unimplemented(opcode, chip.pc);
                            return 1;
                    }
                    break;
                case 0x9:
                    if (chip.v_registers[x] != chip.v_registers[y]) chip.pc += 2;
                    break;
                case 0xA:
                    chip.I = NNN;
                    break;
                case 0xB:
                    chip.pc = NNN + chip.v_registers[0];
                    break;
                case 0xC:
                    chip.v_registers[x] = rand() & NN;
                    break;
                case 0xD: {
                    uint8_t px = chip.v_registers[x] % SCREEN_W;
                    uint8_t py = chip.v_registers[y] % SCREEN_H;
                    chip.v_registers[0xF] = 0;

                    for (int row = 0; row < N; row++) {
                        uint8_t sprite_byte = chip.memory[chip.I + row];
                        for (int col = 0; col < 8; col++) {
                            if (px + col >= SCREEN_W || py + row >= SCREEN_H) {
                                continue;
                            }
                            if ((sprite_byte >> (7 - col)) & 1) {
                                int idx = (py + row) * SCREEN_W + (px + col);
                                if (chip.display[idx]) {
                                    chip.v_registers[0xF] = 1;
                                }
                                chip.display[idx] ^= 1;
                            }
                        }
                    }
                    needs_redraw = 1;
                    break;
                }
                case 0xE:
                    switch (NN) {
                        case 0x9E:  
                            if (chip.keypad[chip.v_registers[x] & 0xF]) chip.pc += 2;
                            break;
                        case 0xA1:  
                            if (!chip.keypad[chip.v_registers[x] & 0xF]) chip.pc += 2;
                            break;
                        default:
                            report_unimplemented(opcode, chip.pc);
                            return 1;
                    }
                    break;
                case 0xF:
                    switch (NN) {
                        case 0x07: chip.v_registers[x] = chip.delay_timer; break;
                        case 0x0A: {  
                            int pressed = -1;
                            for (int k = 0; k < 16; k++) {
                                if (chip.keypad[k]) { pressed = k; break; }
                            }
                            if (pressed >= 0) {
                                chip.v_registers[x] = (uint8_t)pressed;
                            } else {
                                chip.pc -= 2;
                            }
                            break;
                        }
                        case 0x15: chip.delay_timer = chip.v_registers[x]; break;
                        case 0x18: chip.sound_timer = chip.v_registers[x]; break;
                        case 0x1E: chip.I += chip.v_registers[x]; break;
                        case 0x29:
                            chip.I = FONT_START + (chip.v_registers[x] & 0xF) * 5;
                            break;
                        case 0x33:
                            chip.memory[chip.I]     = chip.v_registers[x] / 100;
                            chip.memory[chip.I + 1] = (chip.v_registers[x] / 10) % 10;
                            chip.memory[chip.I + 2] = chip.v_registers[x] % 10;
                            break;
                        case 0x55:
                            for (uint8_t i = 0; i <= x; i++) {
                                chip.memory[chip.I + i] = chip.v_registers[i];
                            }
                            break;
                        case 0x65:
                            for (uint8_t i = 0; i <= x; i++) {
                                chip.v_registers[i] = chip.memory[chip.I + i];
                            }
                            break;
                        default:
                            report_unimplemented(opcode, chip.pc);
                            return 1;
                    }
                    break;
                default:
                    report_unimplemented(opcode, chip.pc);
                    return 1;
            }
        }

        if (chip.delay_timer > 0) chip.delay_timer--;
        if (chip.sound_timer > 0) chip.sound_timer--;
        if (needs_redraw) {
            render(chip.display);
            needs_redraw = 0;
        }
        next_frame += 1.0 / 60.0;
        while (now_sec() < next_frame) Sleep(1);
        if (now_sec() - next_frame > 0.1) next_frame = now_sec(); 
    }

    return 0;
}