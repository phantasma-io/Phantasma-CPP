CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -fpermissive
INCLUDES := -I../Basic\ Examples/CurlRapidjsonSample
LIBCURL ?= $(shell ldconfig -p | awk '/libcurl\.so\./ {print $$NF; exit}')
TARGET := wallet

ifeq ($(LIBCURL),)
LIBS := -lcurl -lcrypto -lpthread
else
LIBS := $(LIBCURL) -lcrypto -lpthread
endif

all: $(TARGET)

$(TARGET): program.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) program.cpp $(LIBS) -o $(TARGET)

.PHONY: clean
clean:
	rm -f $(TARGET)
