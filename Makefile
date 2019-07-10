CXXFLAGS = -std=c++17 -Wall -Werror -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable -march=native
CC = g++
LD = $(CC)

ifeq (${RELEASE}, y)
	CXXFLAGS += -O2 -flto
	LDFLAGS += -flto
else
	CXXFLAGS += -g3 -O0
	LDFLAGS += -g3 -O0
endif

sources = $(wildcard *.cpp)
objs = $(patsubst %.c, ./%.o, $(sources))


all: life compressed_life simd_life

life: life.o

compressed_life: compressed_life.o

simd_life: simd_life.o

%.o: %.c
	$(CC) $(CXXFLAGS) -c $^ -o $@

clean:
	rm *.o life compressed_life simd_life
