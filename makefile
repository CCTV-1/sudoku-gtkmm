CURL_FLAGS=$(shell pkg-config --cflags --libs libcurl)
JANSSON_FLAGS=$(shell pkg-config --cflags --libs jansson)
GTKMM_FLAGS=$(shell pkg-config --cflags --libs gtkmm-3.0)

CC++ =g++
ifndef DEBUG
	CPP_OPTION=-Wall -Wextra -Wpedantic -std=gnu++17 -O3 -m64
endif
ifdef DEBUG
	CPP_OPTION=-Wall -Wextra -Wpedantic -std=gnu++17 -O0 -g3 -pg -m64
endif

sudoku : src/main.cpp sudoku.o dancinglinks.o
	$(CC++) src/main.cpp sudoku.o dancinglinks.o $(CPP_OPTION) $(CURL_FLAGS) $(JANSSON_FLAGS) $(GTKMM_FLAGS) -o sudoku

sudoku.o : src/sudoku.cpp src/sudoku.h
	$(CC++) src/sudoku.cpp $(CPP_OPTION) -c

dancinglinks.o: src/dancinglinks.cpp src/dancinglinks.h
	$(CC++) src/dancinglinks.cpp $(CPP_OPTION) -c

clean :
	-rm sudoku dancinglinks.o sudoku.o
