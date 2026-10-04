#!/usr/bin/make -f
# MultiPlay 20/20 - top-level build
#
#   make                      build bin/nhe-multiplay.lv2 (DSP + TTL + pedal face)
#   make install DESTDIR=...  install into $(DESTDIR)$(PREFIX)/lib/lv2
#   make clean
#
# Cross builds take the usual CC/CXX/CXXFLAGS from the environment, which is
# what mod-plugin-builder and builder.mod.audio pass in.

PREFIX  ?= /usr/local
BUNDLE  := nhe-multiplay.lv2

all: plugin bundle

plugin:
	$(MAKE) -C plugins/multiplay

bundle: plugin
	cp -r bundle/$(BUNDLE)/. bin/$(BUNDLE)/

install: all
	install -d $(DESTDIR)$(PREFIX)/lib/lv2/$(BUNDLE)
	cp -r bin/$(BUNDLE)/. $(DESTDIR)$(PREFIX)/lib/lv2/$(BUNDLE)/

clean:
	$(MAKE) -C plugins/multiplay clean
	rm -rf bin build

.PHONY: all plugin bundle install clean
