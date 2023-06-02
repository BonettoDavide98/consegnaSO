all: master porto nave timer

master: master.c
	gcc -std=c89 -Wpedantic -D_GNU_SOURCE master.c -o master -lm

porto: porto.c
	gcc -std=c89 -Wpedantic -D_GNU_SOURCE porto.c -o porto

nave: nave.c
	gcc -std=c89 -Wpedantic -D_GNU_SOURCE nave.c -o nave -lm

timer: timer.c
	gcc -std=c89 -Wpedantic -D_GNU_SOURCE timer.c -o timer

