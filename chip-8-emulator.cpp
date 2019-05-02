#include <iostream>
#include <fstream>
#include <SDL2/SDL.h>
#include <chrono>
#include <thread>
#include <signal.h>
#include <map>

#define FLAG_REG 0xF
#define SCREEN_WIDTH 64
#define SCREEN_HEIGHT 32
#define SPRITES_WIDTH 8

unsigned char fontset[80] = 
{ 
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F 
};

std::map<int, int> createMap() {
    std::map<int, int> m;
    //m['q'] = 1;
    //m['w'] = 2;
    m['w'] = 3; //up
    //m['r'] = 0xC;

    //m['a'] = 4;
    //m['s'] = 5;
    m['s'] = 6; //Down
    //m['f'] = 0xD;

    m['a'] = 7; //left
    m['d'] = 8; //right
    //m['c'] = 9;
    //m['v'] = 0xE;    
    
    //m['g'] = 0xA;
    //m['h'] = 0;
    //m['j'] = 0xB;
    //m['k'] = 0xF;

    return m;
}

std::map<int, int> key_to_keypad_map = createMap();

struct Regs {
    uint8_t V[16];
    uint16_t I; //memory addresses, only 12 bits used since memory 0-4095

    uint16_t pc;
    uint8_t sp;
};

struct Regs registers;
uint8_t memory[4096];
uint16_t stack[16];
uint8_t delay_timer, sound_timer;
uint8_t keypad[16];

uint8_t screen[SCREEN_WIDTH * SCREEN_HEIGHT];
bool drawFlag;
int lastPressedKey = -1;
void big_to_small_endian(char *buf, int size) {
    uint8_t buf2[size];
    for(int x = 0; x < size; x++) {
        buf2[x] = buf[x];
    }

    for(int x = 0; x < size; x++) {
        buf[x] = buf2[(size - 1) - x];
    }
}

bool setPixel(uint32_t x, uint32_t y) {
    x %= SCREEN_WIDTH;
    y %= SCREEN_HEIGHT;

    if(x < 0) {
        x += SCREEN_WIDTH;
    }
    if(y < 0) {
        y += SCREEN_HEIGHT;
    }

    screen[x + (y * SCREEN_WIDTH)] ^= 1;

    return screen[x + y * SCREEN_WIDTH] == 0;
}

void loadProgram(const char *filename) {
    FILE *file = fopen(filename, "rb");

    int block_size = 1024;
    int index = 0x200;
    int read_amount = block_size;
    while(read_amount == block_size) {
        read_amount = fread(&(memory[index]), 1, block_size, file);
        index += read_amount;
    }
    fclose(file);


    //Load fontset
    for(int i = 0; i < 80; i++) {
        memory[i] = fontset[i];
    }
}

void print_registers(uint16_t opcode) {
    printf("***********************************************\n");
    for(int x = 0; x < 16; x++) {
        printf("V%X: 0x%02X ", x, registers.V[x]);
    }
    printf("\n");
    printf("PC: 0x%04X OPCODE: 0x%04X I: 0x%04X\n", registers.pc, opcode, registers.I);

    printf("Stack\n");
    for(int x = 0; x < 16; x++) {
        if(x == registers.sp) {
            printf("-->");
        } else {
            printf("   ");
        }
        printf("0x%04X\n", stack[x]);
    }
    printf("***********************************************\n");
}

void update_physical_key_presses() {
    SDL_Event e;
    while(true) {
        SDL_WaitEvent(&e);
        int pressed = 0;
        if(e.type == SDL_KEYDOWN) {
            pressed = 1;
        } else if(e.type == SDL_KEYUP) {
            pressed = 0;
        }

        char key = e.key.keysym.sym;
        if(key_to_keypad_map.find(key) != key_to_keypad_map.end()) {
            int keypadKey = key_to_keypad_map.find(key)->second;
            keypad[keypadKey] = pressed;
            if(pressed) {
                lastPressedKey = keypadKey;
            }
        }
        //printf("Key: %c\n", e.key.keysym.sym);
        //while(SDL_PollEvent(&e) != 0);
    }
}

void make_sound() {
    //TODO
}

bool debug = false;
void run_iteration() {
    //fetch
    uint16_t opcode = (memory[registers.pc] << 8) | memory[registers.pc + 1];

    if(registers.pc == 0x07AA) {
        //debug = true;
    }
    //print_registers(opcode);
    if(debug) {
        char test[100];
        std::cin >> test;
    }
    

    registers.pc += 2;

    uint8_t x  = (opcode & 0x0F00) >> 8;
    uint8_t y  = (opcode & 0x00F0) >> 4;
    uint8_t n  = (opcode & 0x000F);
    uint8_t kk = (opcode & 0x00FF);
    uint16_t nnn = (opcode & 0x0FFF);

    //decode & execute
    switch(opcode & 0xF000) {
        case 0x0000:
        {
            if(opcode == 0x00E0) {
                //clear screen
            } else if(opcode == 0x00EE) {
                //return from subroutine
                registers.sp -= 1;
                registers.pc = stack[registers.sp];
            }
            break;
        }
        case 0x1000: //Jump to 0nnn
        {
            registers.pc = nnn;
            break;
        }
        case 0x2000: //Call 0nnn
        {
            stack[registers.sp] = registers.pc;
            registers.sp += 1;
            registers.pc = nnn;
            break;
        }
        case 0x3000: //3xkk, skip next instruction if V[x] == kk
        {
            if(registers.V[x] == kk) {
                registers.pc += 2;
            }
            break;
        }
        case 0x4000: //4xkk, skip next instruction if V[x] != kk
        {
            if(registers.V[x] != kk) {
                registers.pc += 2;
            }
            break;
        }
        case 0x5000: //5xy0 skip next instruction if Vx == Vy
        {
            if((opcode & 0xF) == 0x0) {
                if(registers.V[x] == registers.V[y]) {
                    registers.pc += 2;
                }
            } else {
                printf("Unhandled opcode 0x%X\n", opcode);
            }
            break;
        }
        case 0x6000: //6xkk LD Vx, byte
        {
            registers.V[x] = kk;
            break;
        }
        case 0x7000: //7xkk Vx = Vx + kk
        {
            registers.V[x] += kk;
            break;
        }
        case 0x8000:
        {
            switch(opcode & 0xF) {
                case 0x0: //8xy0 LD Vx, Vy
                registers.V[x] = registers.V[y];
                break;
                case 0x1:
                registers.V[x] |= registers.V[y];
                break;
                case 0x2:
                registers.V[x] &= registers.V[y];
                break;
                case 0x3:
                registers.V[x] ^= registers.V[y];
                break;
                case 0x4:
                if((0xFF - registers.V[x]) < registers.V[y]) {
                    registers.V[FLAG_REG] = 1;
                } else {
                    registers.V[FLAG_REG] = 0;
                }
                registers.V[x] += registers.V[y];
                break;
                case 0x5:
                registers.V[FLAG_REG] = (registers.V[x] > registers.V[y]) ? 1 : 0;
                registers.V[x] -= registers.V[y];
                break;
                case 0x6:
                registers.V[FLAG_REG] = registers.V[x] & 0x1;
                registers.V[x] >>= 1;
                break;
                case 0x7:
                registers.V[FLAG_REG] = (registers.V[y] > registers.V[x]) ? 1 : 0;
                registers.V[x] = registers.V[y] - registers.V[x];
                break;
                case 0xE:
                registers.V[FLAG_REG] = (registers.V[x] & 0x80) >> 7;
                registers.V[x] <<= 1;
                break;
            }
            break;
        }
        case 0x9000:
        {
            if((opcode & 0x1) == 0x0) { 
                if(registers.V[x] != registers.V[y]) {
                    registers.pc += 2;
                } else {
                    printf("Unhandled opcode 0x%X\n", opcode);
                }
            }
            break;
        }
        case 0xA000:
        {
            registers.I = nnn;
        }
        break;
        case 0xB000:
        {
            registers.pc = nnn + registers.V[0];
            break;
        }
        case 0xC000:
        {
            registers.V[x] = (std::rand() % 256) & kk;
        }
        break;
        case 0xD000:
        {
            uint8_t origX = registers.V[x];
            uint8_t origY = registers.V[y];
            uint8_t height = n;

            registers.V[FLAG_REG] = 0;
            //printf("x: %d, y: %d, h: %d\n", origX, origY, height);
            //printf("I %d\n", registers.I);
            for(uint8_t sprite_y = 0; sprite_y < height; sprite_y++) {
                uint8_t x_bitfield = memory[registers.I + sprite_y];
                for(uint8_t sprite_x = 0; sprite_x < SPRITES_WIDTH; sprite_x++) {
                    if((x_bitfield & (0x80 >> sprite_x)) != 0) {
                        if(setPixel(origX + sprite_x, origY + sprite_y)) {
                            registers.V[FLAG_REG] = 1;
                        }
                    }
                }
            }
        break;
        }
        case 0xE000:
        {
            switch (opcode & 0xFF)
            {
                case 0x9E:
                    if(keypad[registers.V[x]] == 1) {
                        registers.pc += 2;
                    }
                    break;
                case 0xA1:
                {
                    if(keypad[registers.V[x]] == 0) {
                        registers.pc += 2;
                    }
                    break;
                }
                default:
                {
                    printf("Unhandled opcode 0x%X\n", opcode);
                    break;
                }
            }
            break;
        }
        case 0xF000:
        {
            switch(opcode & 0xFF) {
                case 0x07:
                {
                    registers.V[x] = delay_timer;
                    break;   
                }
                case 0x0A:
                {
                    //TODO use conditional variable
                    while(lastPressedKey == -1) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    registers.V[x] = lastPressedKey;
                    lastPressedKey = -1;
                    break;
                }
                case 0x15:
                {
                    delay_timer = registers.V[x];
                    break;   
                }
                case 0x18:
                {
                    sound_timer = registers.V[x];
                    break;   
                }
                case 0x1E:
                {
                    registers.V[FLAG_REG] = ((0xFFFF - registers.I) < registers.V[x]) ? 1 : 0;
                    registers.I += registers.V[x];
                    break;   
                }
                case 0x29:
                {
                    registers.I = registers.V[x] * 5;
                    break;   
                }
                case 0x33:
                {
                    memory[registers.I] = (registers.V[x] % 1000) / 100; 
                    memory[registers.I + 1] = (registers.V[x] % 100) / 10; 
                    memory[registers.I + 2] = (registers.V[x] % 10); 
                    break;   
                }
                case 0x55:
                {
                    for(int i = 0; i <= x; i++) {
                        memory[registers.I + i] = registers.V[i];
                    }
                    break;   
                }
                case 0x65:
                {
                    for(int i = 0; i <= x; i++) {
                        registers.V[i] = memory[registers.I + i];
                    }
                    break;   
                }
                default:
                {
                    printf("Unhandled opcode 0x%X\n", opcode);
                    break;
                }
            }
            break;
        }
        default:
        {
            printf("Unhandled opcode 0x%X\n", opcode);
            break;
        }
    }

    if(delay_timer > 0) {
        delay_timer--;
    }
    if(sound_timer > 0) {
        make_sound();
        sound_timer--;
    }
}

void updateScreen(SDL_Renderer *renderer, SDL_Texture *sdlTexture) {
    uint32_t pixels[64 * 32];
    for(int x = 0 ; x < 2048; x++) {
        pixels[x] = (0x00FFFFFF * screen[x]) | 0xFF000000;
    }
    SDL_UpdateTexture(sdlTexture, NULL, pixels, 64 * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, sdlTexture, NULL, NULL);
    SDL_RenderPresent(renderer);
}
void stop(int signum) {
    exit(0);
}
int CHIP8_FREQUENCY = 540;
int chip8(const char *filename) {
    memset(memory, 0, sizeof(memory));
    memset(keypad, 0, sizeof(keypad));
    delay_timer = 0;
    sound_timer = 0;
    registers.pc = 0x200;

    loadProgram(filename);

    //CHIP-8 runs at 60 Hz, so 1 instruction per 16.666 milliseconds

    while(true){
        run_iteration();
        std::this_thread::sleep_for(std::chrono::milliseconds((uint16_t)(1.0/CHIP8_FREQUENCY * 1000)));
    }
}
int main(int argc, char **argv) {
    signal(SIGINT, stop);

    if(argc == 1) {
        printf("Usage: %s ROM_FILE\n", argv[0]);
        exit(-1);
    }
    const char *filename = argv[1];

    int scale = 8;
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow("Chip-8", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH * scale, SCREEN_HEIGHT * scale, SDL_WINDOW_SHOWN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
    SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    SDL_Texture *sdlTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 64, 32);

    std::thread t1(chip8, filename);
    std::thread input_thread(update_physical_key_presses);
    while(true) {
        updateScreen(renderer, sdlTexture);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    //Does not get here
    t1.join();
    return 0;
}