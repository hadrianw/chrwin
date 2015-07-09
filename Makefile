all:
	gcc chrwin.c -o chrwin -std=c99 -D_BSD_SOURCE -D_GNU_SOURCE -Wall -Wextra -pedantic -g -O0
