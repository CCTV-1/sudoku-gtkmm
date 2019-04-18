CURL_FLAGS=$(shell pkg-config --cflags --libs libcurl)
JANSSON_FLAGS=$(shell pkg-config --cflags --libs jansson)
GTKMM_FLAGS=$(shell pkg-config --cflags --libs gtkmm-3.0)
CPP_OPTION=-Wall -Wextra -Wpedantic -std=gnu++17 -m64 -lstdc++fs
CC++ =g++
ifndef DEBUG
	CPP_OPTION+=-O3
endif
ifdef DEBUG
	CPP_OPTION+=-O0 -g3 -pg
endif

sudoku : src/main.cpp sudoku.o dancinglinks.o
	$(CC++) src/main.cpp sudoku.o dancinglinks.o $(CPP_OPTION) $(CURL_FLAGS) $(JANSSON_FLAGS) $(GTKMM_FLAGS) -o sudoku

sudoku.o : src/sudoku.cpp src/sudoku.h
	$(CC++) src/sudoku.cpp $(CPP_OPTION) -c

dancinglinks.o: src/dancinglinks.cpp src/dancinglinks.h
	$(CC++) src/dancinglinks.cpp $(CPP_OPTION) -c

clean :
	-rm sudoku dancinglinks.o sudoku.o
