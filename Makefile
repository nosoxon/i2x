# https://musl.cc/
# CC	= /home/nosoxon/source/i2x/build/armv7l-linux-musleabihf-cross/bin/armv7l-linux-musleabihf-gcc
CC	= /home/nosoxon/source/i2x/build/armv5l-linux-musleabi-cross/bin/armv5l-linux-musleabi-gcc
# CC = gcc
LEX	= flex
YACC	= bison -vd
RM	= rm -f

# CFLAGS	= -static -g -Wall -mfpu=vfpv4-d16 -mfloat-abi=hard -mthumb -march=armv7-a
CFLAGS	= -static -g -Wall  -mfloat-abi=soft -mthumb -march=armv5t

i2x: main.c i2x.c i2x.tab.c i2x.yy.c tree.c exec.c i2x.h i2x.tab.h i2x.yy.h config.h
	$(CC) $(CFLAGS) -o $@ $(filter %.c, $^)


%.tab.c %.tab.h: %.y
	$(YACC) $^

%.yy.c %.yy.h: %.l
	$(LEX) -o $*.yy.c --header-file=$*.yy.h $^

clean:
	$(RM) i2x.tab.* i2x.yy.* i2x.output i2x

.SUFFIXES:
