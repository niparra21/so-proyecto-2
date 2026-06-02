CC := gcc
CXX := g++
CFLAGS := -std=c11 -Wall -Wextra -pedantic -O2
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -O2

TARGET := jqindex
OBJECTS := libjsonindex.o jqindex.o

.PHONY: all clean test doc

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS)

libjsonindex.o: libjsonindex.c libjsonindex.h
	$(CC) $(CFLAGS) -c libjsonindex.c

jqindex.o: jqindex.cpp libjsonindex.h
	$(CXX) $(CXXFLAGS) -c jqindex.cpp

test: $(TARGET)
	sh tests/test_cli.sh

doc:
	/Users/nicole/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3 scripts/build_documentation.py

clean:
	rm -f $(TARGET) $(OBJECTS) examples/*.jnx docs/documentacion.pdf
