.PHONY: all build run clean

CXX ?= g++
CXXFLAGS ?= -std=c++17 -I../include
LDFLAGS ?= -lcrypto
LDFLAGS += -lsodium

TARGET := carbon_tests
SRCS := $(wildcard *.cpp)
BUILD_DIR ?= build
BUILD_STAMP := $(BUILD_DIR)/.dir
OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRCS))
TARGET_PATH := $(BUILD_DIR)/$(TARGET)

all: build

build: $(TARGET_PATH)

$(TARGET_PATH): $(OBJS) | $(BUILD_STAMP)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET_PATH) $(LDFLAGS)

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_STAMP)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_STAMP):
	mkdir -p $(BUILD_DIR)
	touch $(BUILD_STAMP)

run: $(TARGET_PATH)
	./$(TARGET_PATH)

clean:
	rm -rf $(BUILD_DIR)
