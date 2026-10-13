CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g

main: main.c
	$(CC) $(CFLAGS) main.c -o main -lwinmm
	