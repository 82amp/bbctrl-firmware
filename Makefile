DIR := $(shell dirname $(lastword $(MAKEFILE_LIST)))

NODE_MODS  := $(DIR)/node_modules
JADE       := $(NODE_MODS)/jade/bin/jade.js
STYLUS     := $(NODE_MODS)/stylus/bin/stylus
AP         := $(NODE_MODS)/autoprefixer/autoprefixer
BROWSERIFY := $(NODE_MODS)/browserify/bin/cmd.js

TARGET     := build/http
HTML       := index
HTML       := $(patsubst %,$(TARGET)/%.html,$(HTML))
CSS        := $(wildcard src/stylus/*.styl)
CSS_ASSETS := build/css/style.css
JS         := $(wildcard src/js/*.js)
JS_ASSETS  := $(TARGET)/js/assets.js
STATIC     := $(shell find src/resources -type f)
STATIC     := $(patsubst src/resources/%,$(TARGET)/%,$(STATIC))
TEMPLS     := $(wildcard src/jade/templates/*.jade)

AVR_FIRMWARE := src/avr/bbctrl-avr-firmware.hex
GPLAN_MOD := rpi-share/camotics/gplan.so
GPLAN_TARGET := src/py/camotics/gplan.so
GPLAN_IMG := gplan-dev.img

RSYNC_EXCLUDE := \*.pyc __pycache__ \*.egg-info \\\#* \*~ .\\\#\*
RSYNC_EXCLUDE := $(patsubst %,--exclude %,$(RSYNC_EXCLUDE))
RSYNC_OPTS := $(RSYNC_EXCLUDE) -rv --no-g --delete --force

VERSION := $(shell sed -n 's/^.*"version": "\([^"]*\)",.*$$/\1/p' package.json)
PKG_NAME := bbctrl-$(VERSION)
PUB_PATH := root@buildbotics.com:/var/www/buildbotics.com/bbctrl

SUBPROJECTS := avr boot pwr jig

ifndef HOST
HOST=bbctrl.local
endif

ifndef PASSWORD
PASSWORD=buildbotics
endif

ifndef DEST
DEST=mnt
endif

WATCH := src/jade src/jade/templates src/stylus src/js src/resources Makefile

all: html css js static
	@$(MAKE) -C src/avr
	@$(MAKE) -C src/boot
	@$(MAKE) -C src/pwr
	@$(MAKE) -C src/jig

copy: pkg
	rsync $(RSYNC_OPTS) pkg/$(PKG_NAME)/ $(DEST)/bbctrl/

pkg: all $(AVR_FIRMWARE)
	./setup.py sdist

beta-pkg: pkg
	cp dist/$(PKG_NAME).tar.bz2 dist/$(PKG_NAME)-beta.tar.bz2

gplan: $(GPLAN_TARGET)

$(GPLAN_TARGET): $(GPLAN_MOD)
	cp $< $@

$(GPLAN_MOD): $(GPLAN_IMG)
	./scripts/gplan-init-build.sh
	git -C rpi-share/cbang fetch
	git -C rpi-share/cbang reset --hard FETCH_HEAD
	git -C rpi-share/camotics fetch
	git -C rpi-share/camotics reset --hard FETCH_HEAD
	cp ./scripts/gplan-build.sh rpi-share/
	sudo ./scripts/rpi-chroot.sh $(GPLAN_IMG) /mnt/host/gplan-build.sh

$(GPLAN_IMG):
	./scripts/gplan-init-build.sh

.PHONY: $(AVR_FIRMWARE)
$(AVR_FIRMWARE):
	$(MAKE) -C src/avr

publish: pkg
	echo -n $(VERSION) > dist/latest.txt
	rsync $(RSYNC_OPTS) dist/$(PKG_NAME).tar.bz2 dist/latest.txt $(PUB_PATH)/

publish-beta: beta-pkg
	echo -n $(VERSION)-beta > dist/latest-beta.txt
	rsync $(RSYNC_OPTS) dist/$(PKG_NAME)-beta.tar.bz2 dist/latest-beta.txt \
	  $(PUB_PATH)/

update: pkg
	http_proxy= curl -i -X PUT -H "Content-Type: multipart/form-data" \
	  -F "firmware=@dist/$(PKG_NAME).tar.bz2" -F "password=$(PASSWORD)" \
	  http://$(HOST)/api/firmware/update

mount:
	mkdir -p $(DEST)
	sshfs bbmc@$(HOST): $(DEST)

umount:
	fusermount -u $(DEST)

html: templates $(HTML)

css: $(CSS_ASSETS) $(CSS_ASSETS).sha256
	rm -f $(TARGET)/css/style-*.css
	install -D $< $(TARGET)/css/style-$(shell cat $(CSS_ASSETS).sha256).css

js: $(JS_ASSETS) $(JS_ASSETS).sha256
	rm -f $(TARGET)/js/assets-*.js
	install -D $< $(TARGET)/js/assets-$(shell cat $(JS_ASSETS).sha256).js

static: $(STATIC)

templates: build/templates.jade

build/templates.jade: $(TEMPLS)
	mkdir -p build
	cat $(TEMPLS) >$@

build/hashes.jade: $(CSS_ASSETS).sha256 $(JS_ASSETS).sha256
	echo "- var css_hash = '$(shell cat $(CSS_ASSETS).sha256)'" > $@
	echo "- var js_hash = '$(shell cat $(JS_ASSETS).sha256)'" >> $@

$(TARGET)/index.html: build/templates.jade build/hashes.jade

$(JS_ASSETS): $(JS) node_modules
	@mkdir -p $(shell dirname $@)
	$(BROWSERIFY) src/js/main.js -s main -o $@ || \
	(rm -f $@; exit 1)

node_modules:
	npm install

%.sha256: %
	mkdir -p $(shell dirname $@)
	sha256sum $< | sed 's/^\([a-f0-9]\+\) .*$$/\1/' > $@

$(TARGET)/%: src/resources/%
	install -D $< $@

$(TARGET)/%.html: src/jade/%.jade $(wildcard src/jade/*.jade) node_modules
	@mkdir -p $(shell dirname $@)
	$(JADE) -P $< -o $(TARGET) || (rm -f $@; exit 1)

build/css/%.css: src/stylus/%.styl node_modules
	mkdir -p $(shell dirname $@)
	$(STYLUS) < $< > $@ || (rm -f $@; exit 1)

pylint:
	pylint3 -E $(shell find src/py -name \*.py)

jshint:
	./node_modules/jshint/bin/jshint --config jshint.json src/js/*.js

lint: pylint jshint

watch:
	@clear
	$(MAKE)
	@while sleep 1; do \
	  inotifywait -qr -e modify -e create -e delete \
		--exclude .*~ --exclude \#.* $(WATCH); \
	  clear; \
	  $(MAKE); \
	done

tidy:
	rm -f $(shell find "$(DIR)" -name \*~)

clean: tidy
	rm -rf build html dist
	@for SUB in $(SUBPROJECTS); do \
	  $(MAKE) -C src/$$SUB clean; \
	done

dist-clean: clean
	rm -rf node_modules

.PHONY: all install html css static templates clean tidy copy mount umount pkg
.PHONY: gplan lint pylint jshint
