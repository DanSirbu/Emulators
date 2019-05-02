all:
	g++ -g *.cpp -lSDL2 -std=c++11 -pthread -o emulator
	./emulator