mytsh:  scanner.l parser.y mytsh.c
	bison -d parser.y
	flex scanner.l
	gcc -Wall -W -pedantic -ggdb -o $@ mytsh.c parser.tab.c lex.yy.c -lreadline -lfl

clean:
	rm -f mytsh parser.tab.c parser.tab.h  lex.yy.c
