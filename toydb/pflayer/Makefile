CFLAGS ?= -std=gnu89

#PUBLICDIR= /usr0/cs564/public/project
SRC= buf.c hash.c pf.c
OBJ= buf.o hash.o pf.o
HDR = pftypes.h pf.h 

pflayer.o: $(OBJ)
	ld -r -o pflayer.o $(OBJ)

tests: testhash testpf testbufsize testphase1 testiofail

testpf: testpf.o pflayer.o
	cc -o testpf testpf.o pflayer.o

testhash: testhash.o pflayer.o
	cc -o testhash testhash.o pflayer.o

$(OBJ): $(HDR)

testhash.o: $(HDR)

testpf.o: $(HDR)

lint: 
	lint $(SRC)

install: pflayer.o 


testbufsize: testbufsize.o pflayer.o
	$(CC) $(CFLAGS) -o $@ testbufsize.o pflayer.o

testbufsize.o: $(HDR)

.PHONY: tests check
check:
	sh ./check.sh

testphase1: testphase1.o pflayer.o
	$(CC) $(CFLAGS) -o $@ testphase1.o pflayer.o

testphase1.o: $(HDR) testutil.h

testiofail: testiofail.o pflayer.o
	$(CC) $(CFLAGS) -o $@ testiofail.o pflayer.o -Wl,--wrap=write -Wl,--wrap=read

testiofail.o: $(HDR) testutil.h

workload: workload.o pflayer.o
	$(CC) $(CFLAGS) -o $@ workload.o pflayer.o

workload.o: pf.h
