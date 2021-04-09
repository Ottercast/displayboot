CC ?= gcc
RM = rm -f

CCFLAGS = -Wall -D_GNU_SOURCE
ifeq ($(DEBUG),1)
	CCFLAGS += -O1 -ggdb
else
	CCFLAGS += -Ofast 
endif

DEPS = freetype2 libpng
DEPFLAGS_CC = `pkg-config --cflags $(DEPS)`
DEPFLAGS_LD = `pkg-config --libs $(DEPS)`
OBJS = $(patsubst %.c,%.o,$(wildcard *.c))
HDRS = $(wildcard *.h)

all: displayboot

%.o : %.c $(HDRS) Makefile
	$(CC) -c $(CCFLAGS) $(DEPFLAGS_CC) $< -o $@

displayboot: $(OBJS)
	$(CC) $(CCFLAGS) $^ $(DEPFLAGS_LD) -o displayboot

clean:
	$(RM) $(OBJS)
	$(RM) displayboot

.PHONY: all clean
