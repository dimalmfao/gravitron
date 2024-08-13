CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11

SDL2_CFLAGS = $(shell sdl2-config --cflags)
SDL2_LDFLAGS = $(shell sdl2-config --libs) -lSDL2_ttf

SRCS = main.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = gravity_simulation

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET) $(SDL2_LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(SDL2_CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
