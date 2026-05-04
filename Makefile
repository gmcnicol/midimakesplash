BUILD_DIR := build
CONFIG := RelWithDebInfo
TARGET := MidiSpash
PLUGIN := Midi Make Splash.vst3
PLUGIN_ARTIFACT := $(BUILD_DIR)/$(TARGET)_artefacts/$(CONFIG)/VST3/$(PLUGIN)
PLUGIN_DEST := $(HOME)/Library/Audio/Plug-Ins/VST3/$(PLUGIN)
REAPER_DIR := $(HOME)/Library/Application Support/REAPER

.PHONY: all configure build deploy clean-reaper-cache verify smoke check-reaper-running clean

all: deploy

configure:
	cmake -S . -B "$(BUILD_DIR)" -G Ninja

build: configure
	cmake --build "$(BUILD_DIR)" --target "$(TARGET)_VST3" --config "$(CONFIG)"

deploy: build smoke check-reaper-running clean-reaper-cache
	@test -d "$(PLUGIN_ARTIFACT)" || (echo "Missing plugin bundle: $(PLUGIN_ARTIFACT)" && exit 1)
	mkdir -p "$(HOME)/Library/Audio/Plug-Ins/VST3"
	rm -rf "$(PLUGIN_DEST)"
	cp -R "$(PLUGIN_ARTIFACT)" "$(PLUGIN_DEST)"
	xattr -dr com.apple.quarantine "$(PLUGIN_DEST)" 2>/dev/null || true
	$(MAKE) verify
	@echo "If REAPER was already open, quit and reopen it. Do not clear/rescan the full VST cache."

smoke: configure
	cmake --build "$(BUILD_DIR)" --target midi_splash_smoke --config "$(CONFIG)"
	"$(BUILD_DIR)/midi_splash_smoke_artefacts/$(CONFIG)/midi_splash_smoke"

check-reaper-running:
	@if pgrep -x REAPER >/dev/null 2>&1; then \
		echo "WARNING: REAPER is running. The on-disk VST3 will be replaced, but open instances can keep the old editor loaded until REAPER is restarted."; \
	fi

clean-reaper-cache:
	@if [ -d "$(REAPER_DIR)" ]; then \
		find "$(REAPER_DIR)" -type f \( -iname "*vst*.ini" -o -iname "*vst*.txt" \) -print0 | \
		while IFS= read -r -d '' f; do \
			if grep -Eiq "Midi[ _]Make[ _]Splash|MidiSpash" "$$f"; then \
				echo "Cleaning REAPER cache: $$f"; \
				cp "$$f" "$$f.bak"; \
				grep -Eiv "Midi[ _]Make[ _]Splash|MidiSpash" "$$f" > "$$f.tmp"; \
				mv "$$f.tmp" "$$f"; \
			fi; \
		done; \
	fi

verify:
	@echo "Installed plugin files:"
	@find "$(PLUGIN_DEST)" -maxdepth 4 -type f -print
	@echo
	@echo "Architecture:"
	@file "$(PLUGIN_DEST)/Contents/MacOS/Midi Make Splash"
	@lipo -info "$(PLUGIN_DEST)/Contents/MacOS/Midi Make Splash"
	@codesign --verify --deep --strict --verbose=2 "$(PLUGIN_DEST)"

clean:
	rm -rf "$(BUILD_DIR)"
