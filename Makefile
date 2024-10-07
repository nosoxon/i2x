CC	= gcc
LEX	= flex
YACC	= bison -vd
RM	= rm -f

CFLAGS	= -g -Wall

i2x: main.c i2x.c i2x.tab.c i2x.yy.c tree.c exec.c i2x.h i2x.tab.h i2x.yy.h config.h
	$(CC) $(CFLAGS) -o $@ $(filter %.c, $^)


%.tab.c %.tab.h: %.y
	$(YACC) $^

%.yy.c %.yy.h: %.l
	$(LEX) -o $*.yy.c --header-file=$*.yy.h $^

clean:
	$(RM) i2x.tab.* i2x.yy.* i2x.output i2x

.SUFFIXES:
