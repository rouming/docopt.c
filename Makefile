CC = gcc
LEX = flex
YACC = bison
CFLAGS = -Wall -O2 -g

all: docopt

# Disable implicit yacc and lex rules
%.c: %.y
%.c: %.l

# docopt

docopt: docopt.tab.c docopt.lex.c docopt.c docopt.h
	$(CC) $(CFLAGS) -g -o docopt docopt.tab.c docopt.lex.c docopt.c

docopt.tab.c docopt.tab.h: docopt.y
	$(YACC) -v -gdocopt.grm.dot -o $@ --defines --debug docopt.y

docopt.lex.c: docopt.l
	$(LEX) -o $@ docopt.l

# Common

png:
	dot -Tpng -odocopt.grm.png docopt.grm.dot
clean:
	rm -f *~ *.output *.grm.* *.tab.* *.lex.* docopt
