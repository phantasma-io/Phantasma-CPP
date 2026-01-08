CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wno-unused-parameter
INCLUDES := -I../../../../../external/rapidjson/include -I../../include -I../Basic\ Examples/CurlRapidjsonSample
LIBS := /lib64/libcurl.so.4 -lcrypto -lpthread
TARGET := carbon-token-deployment-tool

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) main.cpp $(LIBS) -o $(TARGET)

.PHONY: clean
clean:
	rm -f $(TARGET)
