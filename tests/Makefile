.PHONY: all build run clean

CXX ?= g++
CXXFLAGS ?= -std=c++17 -I../include
LDFLAGS ?= -lcrypto

TARGET := carbon_tests
SRCS := $(wildcard *.cpp)

all: build

build: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
