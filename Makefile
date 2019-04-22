myshell:  scanner.l parser.y myshell.c
	bison -d parser.y
	flex scanner.l
	gcc -Wall -W -pedantic -o $@ myshell.c parser.tab.c lex.yy.c -lfl

clean:
	rm myshell parser.tab.c parser.tab.h  lex.yy.c
