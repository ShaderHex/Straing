CXX = g++

BUILD_TYPE ?= release

TARGET = app

CXXFLAGS = -std=c++17 -Wall -Wextra -MMD -MP

ifeq ($(BUILD_TYPE), debug)
	CXXFLAGS += -g -O0 # Add debug symbols, disable optimization
else
	CXXFLAGS += -O2 -flto # Add optimizations and Link-Time Optimization
endif

INCLUDES = -Iglad -Iimgui -Iimgui/backends -Itinyobjloader -Istb

LDLIBS = -lglfw -ldl -lGL


SRCS = main.cpp glad/glad.c \
       imgui/imgui.cpp \
       imgui/imgui_draw.cpp \
       imgui/imgui_tables.cpp \
       imgui/imgui_widgets.cpp \
       imgui/imgui_demo.cpp \
       imgui/backends/imgui_impl_glfw.cpp \
       imgui/backends/imgui_impl_opengl3.cpp

OBJS = $(SRCS:.cpp=.o)
OBJS := $(OBJS:.c=.o)

DEPS = $(OBJS:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "🔗 Linking executable: $@"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	@echo "🔨 Compiling C source: $<"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

%.o: %.cpp
	@echo "🔨 Compiling C++ source: $<"
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

run: all
	./$(TARGET)

clean:
	@echo "🧹 Cleaning up..."
	rm -f $(TARGET) $(OBJS) $(DEPS)

-include $(DEPS)

.PHONY: all run clean