CC := gcc
CXX := g++
CFLAGS := -std=c11 -Wall -Wextra -pedantic -O2
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -O2

TARGET := jqindex
OBJECTS := libjsonindex.o jqindex.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS)

libjsonindex.o: libjsonindex.c libjsonindex.h
	$(CC) $(CFLAGS) -c libjsonindex.c

jqindex.o: jqindex.cpp libjsonindex.h
	$(CXX) $(CXXFLAGS) -c jqindex.cpp


clean:
	rm -f $(TARGET) $(OBJECTS) examples/*.jnx
