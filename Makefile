CC ?= cc
CFLAGS ?= -std=c11 -O2 -pipe -Wall -Wextra
LDFLAGS ?= -lpthread

TARGET := sentinel
SRC := sentinel.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET) /tmp/sentinel-bin-*


