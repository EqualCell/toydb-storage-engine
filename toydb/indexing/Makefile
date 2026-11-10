CC ?= cc
CFLAGS ?= -std=gnu89 -g -Wall -Wextra
LIBS = ../amlayer/amlayer.o ../rmlayer/rmlayer.o ../pflayer/pflayer.o
all: test_index benchmark
../pflayer/pflayer.o:
	$(MAKE) -C ../pflayer pflayer.o
../amlayer/amlayer.o:
	$(MAKE) -C ../amlayer amlayer.o
../rmlayer/rmlayer.o:
	$(MAKE) -C ../rmlayer rmlayer.o
index.o test_index.o benchmark.o: index.h ../amlayer/am.h ../rmlayer/rm.h
index.o: index.c
test_index: test_index.o index.o ../rmlayer/student.o $(LIBS)
	$(CC) $(CFLAGS) -o $@ $^
benchmark: benchmark.o index.o ../rmlayer/student.o $(LIBS)
	$(CC) $(CFLAGS) -o $@ $^
../rmlayer/student.o:
	$(MAKE) -C ../rmlayer student.o
.PHONY: all check
check:
	python3 check.py

test_am: test_am.o index.o ../rmlayer/student.o $(LIBS)
	$(CC) $(CFLAGS) -o $@ $^
test_am.o: index.h
