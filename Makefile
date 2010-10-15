defrag: defrag.c
	cc -Wall -O3 -DDEBUG=$(DEBUG) -o defrag defrag.c

clean:
	rm defrag
