CC ?= cc
CFLAGS ?= -std=gnu89 -Wall -Wextra
OBJ = rm.o common.o fixed.o
all: rmlayer.o test_rm
rmlayer.o: $(OBJ)
	ld -r -o $@ $(OBJ)
../pflayer/pflayer.o:
	$(MAKE) -C ../pflayer pflayer.o
$(OBJ): rm.h internal.h ../pflayer/pf.h
test_rm: test_rm.o rmlayer.o ../pflayer/pflayer.o
	$(CC) $(CFLAGS) -o $@ $^
test_rm.o: rm.h
.PHONY: all check
check:
	sh check.sh

fixed.o: fixed.h

test_records: test_records.o rmlayer.o ../pflayer/pflayer.o
	$(CC) $(CFLAGS) -o $@ $^
test_records.o: rm.h fixed.h internal.h

benchmark: benchmark.o student.o rmlayer.o ../pflayer/pflayer.o
	$(CC) $(CFLAGS) -o $@ $^
benchmark.o: rm.h fixed.h student.h ../pflayer/pftypes.h
student.o: student.h

test_student: test_student.o student.o
	$(CC) $(CFLAGS) -o $@ $^
test_student.o: student.h

test_failures: test_failures.o rmlayer.o ../pflayer/pflayer.o
	$(CC) $(CFLAGS) -o $@ $^ -Wl,--wrap=read -Wl,--wrap=write
test_failures.o: rm.h internal.h fixed.h
