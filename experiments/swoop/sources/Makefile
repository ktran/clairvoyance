BENCHMARKS= CG UA LU


.SECONDEXPANSION:

SWOOP: BINDIR
	$(foreach bench, $(BENCHMARKS), \
	$(MAKE) -C $(bench)/src 2>&1 | tee $(bench)/bin/log.txt;)

BINDIR: $$(foreach bench, $$(BENCHMARKS), $$(bench)/bin)

%/bin:
	mkdir -p $@

clean:
	$(foreach bench, $(BENCHMARKS), \
	$(MAKE) -C $(bench)/src clean;)
