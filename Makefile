CC	= gcc
LEX	= flex
YACC	= bison -vd

CFLAGS	= -g

i2x:
	$(YACC) i2x.y
	$(LEX) i2x.l
	gcc $(CFLAGS) -o i2x i2x.c lex.yy.c i2x.tab.c -ly -lfl

clean:
	rm i2x i2x.tab.* i2x.output lex.yy.c
