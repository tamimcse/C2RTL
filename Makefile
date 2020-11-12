
PLUGIN=onko.so
SOURCES=\
        onko.cc \
		$(END)

include ./Makefile.common

.PHONY: sail
sail: $(PLUGIN)
	$(CCPLUGIN) -c -o /dev/null tests/sail.c

.PHONY: cptrie
cptrie: $(PLUGIN)
	$(CCPLUGIN) -c -o /dev/null tests/cptrie.c

.PHONY: poptrie
poptrie: $(PLUGIN)
	$(CCPLUGIN) -c -o /dev/null tests/poptrie.c

.PHONY: test
test: $(PLUGIN)
	$(CCPLUGIN) -c -o /dev/null tests/test.c
