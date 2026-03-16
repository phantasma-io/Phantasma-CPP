.PHONY: all build run clean

CXX ?= g++
CXXFLAGS ?= -std=c++17 -I../include
LDFLAGS ?= -lcrypto
LDFLAGS += -lsodium

TARGET := carbon_tests
SRCS := $(filter-out vm_bigint_parity_main.cpp,$(wildcard *.cpp))
BUILD_DIR ?= build
BUILD_STAMP := $(BUILD_DIR)/.dir
OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRCS))
TARGET_PATH := $(BUILD_DIR)/$(TARGET)
VM_BIGINT_PARITY_TARGET := $(BUILD_DIR)/vm_bigint_parity_tests
VM_BIGINT_PARITY_SRCS := test_common.cpp vm_bigint_parity_tests.cpp vm_bigint_parity_main.cpp
VM_BIGINT_PARITY_OBJS := $(patsubst %.cpp,$(BUILD_DIR)/vm_bigint_parity_%.o,$(VM_BIGINT_PARITY_SRCS))

all: build

build: $(TARGET_PATH) $(VM_BIGINT_PARITY_TARGET)

$(TARGET_PATH): $(OBJS) | $(BUILD_STAMP)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET_PATH) $(LDFLAGS)

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_STAMP)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(VM_BIGINT_PARITY_TARGET): $(VM_BIGINT_PARITY_OBJS) | $(BUILD_STAMP)
	$(CXX) $(CXXFLAGS) $(VM_BIGINT_PARITY_OBJS) -o $(VM_BIGINT_PARITY_TARGET) $(LDFLAGS)

$(BUILD_DIR)/vm_bigint_parity_%.o: %.cpp | $(BUILD_STAMP)
	$(CXX) $(CXXFLAGS) -DPHANTASMA_EXCEPTION_ENABLE -c $< -o $@

$(BUILD_STAMP):
	mkdir -p $(BUILD_DIR)
	touch $(BUILD_STAMP)

run: $(TARGET_PATH)
	./$(TARGET_PATH)

run-vm-bigint-parity: $(VM_BIGINT_PARITY_TARGET)
	./$(VM_BIGINT_PARITY_TARGET)

clean:
	rm -rf $(BUILD_DIR)
