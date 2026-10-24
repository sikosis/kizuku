CXX ?= c++
VERSION := $(shell tr -d '\r\n' < VERSION)
CPPFLAGS += -DKIZUKU_VERSION=\"$(VERSION)\"
CXXFLAGS ?= -O2 -Wall -Wextra -Wpedantic
CXXFLAGS += -std=c++17

TARGET := kizuku
SOURCES := main.cpp

.PHONY: all clean test install hpkg

all: $(TARGET)

$(TARGET): $(SOURCES) VERSION
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $(SOURCES)

test: $(TARGET)
	./tests/test_generator.sh
	./tests/test_package_script.sh

install: $(TARGET)
	install -d "$(DESTDIR)/boot/home/config/non-packaged/bin"
	install -m 755 $(TARGET) "$(DESTDIR)/boot/home/config/non-packaged/bin/$(TARGET)"

hpkg:
	./scripts/build_hpkg.sh

clean:
	rm -f $(TARGET)
