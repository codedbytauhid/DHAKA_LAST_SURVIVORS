CXX      = g++
CC       = gcc
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -fdiagnostics-color=always -I./include -I./src
CFLAGS   = -O2 -I./include
LDFLAGS  = -Llib -lglfw3 -lopengl32 -lgdi32

OBJDIR   = build/obj
TARGET   = build/main.exe

CPP_SRCS = $(wildcard src/*.cpp) $(wildcard src/*/*.cpp)
CPP_OBJS = $(patsubst src/%.cpp,$(OBJDIR)/%.o,$(CPP_SRCS))
OBJS     = $(CPP_OBJS) $(OBJDIR)/glad.o

win: $(TARGET)
	./$(TARGET)

$(TARGET): $(OBJS) build/glfw3.dll
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

build/glfw3.dll: lib/glfw3.dll
	mkdir -p build
	cp lib/glfw3.dll build/glfw3.dll

$(OBJDIR)/glad.o: src/glad.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: src/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET)

-include $(CPP_OBJS:.o=.d)