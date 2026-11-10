.PHONY: all check sanitize sample
all:
	$(MAKE) -C toydb all
check:
	$(MAKE) -C toydb check
sanitize:
	$(MAKE) -C toydb sanitize
sample:
	python3 scripts/make_sample_data.py
