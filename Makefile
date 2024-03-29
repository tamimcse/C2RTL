PLUGIN=c2rtl.so
SOURCES= c2rtl.cc queue.cc vector.cc $(END)

INSTALLDIR=/usr/bin/gcc-4.8

CC=/usr/bin/gcc
CXX=/usr/bin/g++
PLUGINDIR=$(shell $(CC) -print-file-name=plugin)

CXXFLAGS=-std=gnu++11 -fPIC -Wall -g -fno-rtti -I$(PLUGINDIR)/include
# This is a side effect of using C++11
CXXFLAGS+=-Wno-literal-suffix

LDFLAGS=
LDADD=

END=
OBJECTS=$(patsubst %.cc,%.o,$(SOURCES))

all: $(PLUGIN)

$(PLUGIN): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ -shared $+ $(LDADD)

%.o: %.cc
	$(CXX) -c -o $@ $(CXXFLAGS) $<

PLUGINFLAG=-fplugin=./$(PLUGIN)

CCPLUGIN=$(CC) $(PLUGINFLAG)
CXXPLUGIN=$(CXX)  $(PLUGINFLAG)

clean:
	rm -f $(OBJECTS) $(PLUGIN)

#Test programs

sail_ip6: $(PLUGIN)
	$(CCPLUGIN) -c -o /dev/null tests/sail_ip6.c

sail_ip4: $(PLUGIN)
	$(CCPLUGIN) -c -o /dev/null tests/sail_ip4.c

cptrie_ip6: $(PLUGIN)
	$(CCPLUGIN) -c -o /dev/null tests/cptrie_ip6.c

poptrie_ip6: $(PLUGIN)
	$(CCPLUGIN) -c -o /dev/null tests/poptrie_ip6.c


tabtree: $(PLUGIN)
	$(CCPLUGIN) -c -o /dev/null tests/tabtree.c

cutsplit: $(PLUGIN)
	$(CCPLUGIN) -c -o /dev/null tests/cutsplit.c

hybridcut: $(PLUGIN)
	$(CCPLUGIN) -c -o /dev/null tests/hybridcut.c

ethernet_forwarding: $(PLUGIN)
	$(CCPLUGIN) -c -o /dev/null tests/ethernet_forwarding.c

heavy_hitter_detection: $(PLUGIN)
	$(CCPLUGIN) -c -o /dev/null tests/heavy_hitter_detection.c

rcp: $(PLUGIN)
	$(CCPLUGIN) -c -o /dev/null tests/rcp.c

test: $(PLUGIN)
	$(CCPLUGIN) -c -o /dev/null tests/test.c
