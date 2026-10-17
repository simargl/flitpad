CXX ?= g++
CXXFLAGS = -std=c++11 -Wall -Wextra $(shell fltk-config --cxxflags)
LDFLAGS = $(shell fltk-config --use-images --ldstaticflags)

TARGET = flitpad
SRCS = main.cxx TextEditorApp.cxx FindReplaceDialog.cxx
OBJS = $(SRCS:.cxx=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.cxx
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean