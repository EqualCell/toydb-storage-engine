CFLAGS ?= -std=gnu89

a.out : am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o ../pflayer/pflayer.o main.o amscan.o amprint.o
	$(CC) $(CFLAGS) am.o amfns.o amsearch.o aminsert.o  amstack.o amglobals.o ../pflayer/pflayer.o main.o amscan.o amprint.o 

amlayer.o : am.o amfns.o amsearch.o aminsert.o amstack.o amglobals.o amscan.o amprint.o ambulk.o amvalidate.o
	ld -r am.o amfns.o amsearch.o aminsert.o  amstack.o amglobals.o amscan.o amprint.o ambulk.o amvalidate.o  -o amlayer.o

am.o : am.c am.h pf.h
	$(CC) $(CFLAGS) -c am.c

amfns.o : amfns.c am.h pf.h
	$(CC) $(CFLAGS) -c amfns.c

amsearch.o : amsearch.c am.h pf.h
	$(CC) $(CFLAGS) -c amsearch.c

aminsert.o : aminsert.c am.h pf.h
	$(CC) $(CFLAGS) -c aminsert.c

amscan.o : amscan.c am.h pf.h
	$(CC) $(CFLAGS) -c amscan.c

amstack.o : amstack.c am.h pf.h
	$(CC) $(CFLAGS) -c amstack.c

amglobals.o : amglobals.c am.h
	$(CC) $(CFLAGS) -c amglobals.c

amprint.o : amprint.c am.h pf.h 
	$(CC) $(CFLAGS) -c amprint.c
	
main.o : main.c am.h pf.h 
	$(CC) $(CFLAGS) -c main.c



ambulk.o: ambulk.c am.h pf.h
	$(CC) $(CFLAGS) -c ambulk.c
amvalidate.o: amvalidate.c am.h pf.h
	$(CC) $(CFLAGS) -c amvalidate.c
