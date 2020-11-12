
PLUGIN=onko.so
SOURCES=\
        onko.cc \
		$(END)

include ./Makefile.common

.PHONY: sail
sail: $(PLUGIN)
	$(CCPLUGIN) -c -o /dev/null sail.c

.PHONY: cptrie
cptrie: $(PLUGIN)
	$(CCPLUGIN) -c -o /dev/null cptrie.c

.PHONY: poptrie
poptrie: $(PLUGIN)
	$(CCPLUGIN) -c -o /dev/null poptrie.c

.PHONY: test
test: $(PLUGIN)
	$(CCPLUGIN) -c -o /dev/null test.c
