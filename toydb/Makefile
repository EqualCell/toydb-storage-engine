.PHONY: all check sanitize
all:
	$(MAKE) -B -C indexing all
check:
	$(MAKE) -C pflayer check
	$(MAKE) -C rmlayer check
	$(MAKE) -C indexing check
sanitize:
	ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 INDEX_CFLAGS='-std=gnu89 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer' $(MAKE) -C indexing check
