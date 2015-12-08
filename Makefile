CFLAGS=-Wall -Isrc -Wextra $(OPTFLAGS)
LIBS=-ldl event $(OPTLIBS)
PREFIX?=/usr/local

SOURCES=$(wildcard src/**/*.c src/*.c)
OBJECTS=$(patsubst %.c,%.o,$(SOURCES))

TEST_SRC=$(wildcard test/*_test.c)
TESTS=$(patsubst %.c,%,$(TEST_SRC))

TARGET=build/libdht.a
SO_TARGET=$(patsubst %.a,%.so,$(TARGET))

all: $(TARGET) $(SO_TARGET) tests

dev: CFLAGS=-Wall -Isrc -Wextra $(OPTFLAGS)
	dev: all

$(TARGET): CFLAGS += -fPIC
	$(TARGET): build $(OBJECTS)
	ar rcs $@ $(OBJECTS)
	ranlib $@

$(SO_TARGET): $(TARGET) $(OBJECTS)
	$(CC) -shared -o $@ $(OBJECTS)

build:
	@mkdir -p build
	@mkdir -p bin

clean:
	rm -rf build $(OBJECTS) $(TESTS)
	rm -f tests/tests.log
	find . -name "*.gc*" -exec rm {} \;
	rm -rf `find . -name "*.dSYM" -print`
