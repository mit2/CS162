all: main wc map

main: main.c
	gcc -g -o main main.c
wc: wc.c
	gcc -g -o wc wc.c
map: map.c
	gcc -g -o map map.c
clean:
	rm map wc main

.PHONY: all clean check
