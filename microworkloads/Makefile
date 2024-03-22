M5UTIL=../../gem5/util/m5
FLAGS=-g -O3 -msse4 -Wall -Werror -I$(RLITE)/src -I$(M5UTIL) \
	-DNUM_USERS=$(NUM_USERS) -DNUM_WEEKS_NOVEMBER=$(NUM_WEEKS_NOVEMBER)
CPPFLAGS=$(FLAGS) -std=c++11
CFLAGS=$(FLAGS) -std=c99
LDFLAGS=-static -lm

SRCS = $(wildcard *.c)

PROGS = $(patsubst %.c,%.exe,$(SRCS))

all: $(PROGS)

%.exe: %.c
	gcc -static -O3 -msse2 -I ../gem5/util/m5 $< m5op_x86.S rowop.S -o $@

clean:
	rm -f *.exe

