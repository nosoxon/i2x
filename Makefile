CC	= gcc
LEX	= flex
YACC	= bison -vd
RM	= rm -f

CFLAGS	= -g -Wall

i2x: i2x.c i2x.tab.c i2x.yy.c i2x.h i2x.tab.h
	$(CC) $(CFLAGS) -o $@ $(filter %.c, $^) -ly -lfl


%.tab.c %.tab.h: %.y
	$(YACC) $^

%.yy.c: %.l
	$(LEX) -o $@ $^

clean:
	$(RM) i2x.tab.* i2x.yy.c i2x.output i2x

.SUFFIXES:
