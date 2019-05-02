emulator:
	g++ -g *.cpp -lSDL2 -std=c++11 -pthread -o emulator

run: emulator
	./emulator roms/BLINKY.ch8