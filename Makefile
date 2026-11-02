# Makefile for 3ds_drop (Wireless LAN SD Scanner, Log Harvester & Atomic File Drop Server)

APP_TITLE   := 3DS Drop
APP_DESC    := Wireless SD File Server
APP_AUTHOR  := lafiabrew
APP_CODE    := CTR-P-DROP
TARGET      := 3ds_drop

HOST_CC     := gcc
HOST_CFLAGS := -std=c11 -Wall -Wextra -Werror -O2 -g -Iinclude
SAN_FLAGS   := -fsanitize=address,undefined

CORE_SRCS   := src/drop_server.c

OCI_RUNTIME := $(shell command -v podman 2>/dev/null || command -v docker 2>/dev/null)
OCI_IMAGE   := localhost/pieceduel-3ds-toolchain:latest
OCI_USER    := $(if $(findstring podman,$(OCI_RUNTIME)),--userns=keep-id,-u $(shell id -u):$(shell id -g))

.PHONY: all host test 3ds 3ds-native 3ds-container assets verify-3ds netload qr serve-cia clean

all: test

assets:
	@python3 meta/generate_assets.py

build/host:
	@mkdir -p build/host

build/host/test_drop: $(CORE_SRCS) src/platform_host.c tests/test_drop.c | build/host
	$(HOST_CC) $(HOST_CFLAGS) $(SAN_FLAGS) $(CORE_SRCS) src/platform_host.c tests/test_drop.c -lm -o $@

build/host/$(TARGET)_host: $(CORE_SRCS) src/platform_host.c src/main.c | build/host
	$(HOST_CC) $(HOST_CFLAGS) $(CORE_SRCS) src/platform_host.c src/main.c -lm -o $@

host: build/host/test_drop build/host/$(TARGET)_host

test: host
	./build/host/test_drop
	./build/host/$(TARGET)_host --sd-root build/test_sd --port 0 --once

3ds-native: assets
	@mkdir -p build/3ds
	$(DEVKITARM)/bin/arm-none-eabi-gcc \
		-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft \
		-std=gnu11 -Wall -Wextra -Werror -O2 -ffunction-sections -fdata-sections \
		-D__3DS__ -Iinclude \
		-I$(DEVKITPRO)/libctru/include \
		$(CORE_SRCS) src/platform_3ds.c src/main.c \
		-specs=3dsx.specs \
		-L$(DEVKITPRO)/libctru/lib \
		-Wl,--gc-sections \
		-lcitro2d -lcitro3d -lctru -lm \
		-o build/3ds/$(TARGET).elf
	$(DEVKITPRO)/tools/bin/smdhtool --create "$(APP_TITLE)" "$(APP_DESC)" "$(APP_AUTHOR)" meta/icon.png build/3ds/$(TARGET).smdh
	python3 -c 'import sys; sys.path.insert(0, "meta"); from generate_bnr_shared import patch_smdh_for_cia; patch_smdh_for_cia("build/3ds/$(TARGET).smdh")'
	$(DEVKITPRO)/tools/bin/3dsxtool build/3ds/$(TARGET).elf build/3ds/$(TARGET).3dsx --smdh=build/3ds/$(TARGET).smdh
	@if command -v makerom >/dev/null 2>&1; then \
		makerom -f cia -o build/3ds/$(TARGET).cia -DAPP_ENCRYPTED=false -rsf meta/app.rsf -target t -exefslogo -elf build/3ds/$(TARGET).elf -icon build/3ds/$(TARGET).smdh -banner meta/banner.bnr; \
	fi

3ds-container: assets
	@if ! $(OCI_RUNTIME) image inspect $(OCI_IMAGE) >/dev/null 2>&1; then \
		$(OCI_RUNTIME) build -f Containerfile.3ds -t $(OCI_IMAGE) .; \
	fi
	$(OCI_RUNTIME) run --rm $(OCI_USER) -v "$(CURDIR):/work:Z" -w /work $(OCI_IMAGE) make 3ds-native

3ds:
	@if [ -n "$(DEVKITARM)" ] && [ -x "$(DEVKITARM)/bin/arm-none-eabi-gcc" ]; then \
		$(MAKE) 3ds-native; \
	else \
		$(MAKE) 3ds-container; \
	fi

verify-3ds:
	bash scripts/verify-3ds-build.sh

netload:
	@if [ -z "$(IP)" ]; then echo "Usage: make netload IP=<3ds-ip>"; exit 1; fi
	$(OCI_RUNTIME) run --rm --net=host -v "$(CURDIR):/work:Z" -w /work $(OCI_IMAGE) 3dslink -a "$(IP)" build/3ds/$(TARGET).3dsx

qr serve-cia:
	python3 ../serve_3ds.py $(TARGET) $(if $(IP),--push $(IP),)

clean:
	rm -rf build/ meta/icon.png meta/banner.png meta/audio.wav meta/banner.bnr
