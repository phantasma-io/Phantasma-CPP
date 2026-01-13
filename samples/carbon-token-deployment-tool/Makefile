CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wno-unused-parameter
INCLUDES := -I../../../../../external/rapidjson/include -I../../include -I../Basic\ Examples/CurlRapidjsonSample
SODIUM_CFLAGS := $(shell pkg-config --cflags libsodium)
SODIUM_LIBS := $(shell pkg-config --libs libsodium)
LIBS := /lib64/libcurl.so.4 -lcrypto -lpthread $(SODIUM_LIBS)
TARGET := carbon-token-deployment-tool

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) $(SODIUM_CFLAGS) $(INCLUDES) main.cpp $(LIBS) -o $(TARGET)

.PHONY: clean
clean:
	rm -f $(TARGET)
