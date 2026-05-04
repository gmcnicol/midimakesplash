cmake_minimum_required(VERSION 3.24)

project(midi_spash VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
# ysfx runtime
set(YSFX_PLUGIN OFF CACHE BOOL "" FORCE)
set(YSFX_TESTS OFF CACHE BOOL "" FORCE)
set(YSFX_TOOLS OFF CACHE BOOL "" FORCE)
set(YSFX_SKIP_CHECKSUM ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    ysfx
    GIT_REPOSITORY https://github.com/jpcima/ysfx.git
    GIT_TAG 8077347ccf4115567aed81400281dca57acbb0cc
    GIT_SUBMODULES_RECURSE TRUE
)

FetchContent_MakeAvailable(ysfx)
FetchContent_Declare(
	mrgee_fx
	GIT_REPOSITORY https://github.com/gmcnicol/mrgee-fx.git
	GIT_TAG main
	GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(mrgee_fx)

mrgee_add_jsfx_plugin(
	TARGET MidiSpash
	PRODUCT_NAME "Midi Make Splash"
	PLUGIN_CODE MMS1
	JSFX_FILE "${CMAKE_CURRENT_SOURCE_DIR}/jsfx/WaterSplash.jsfx"
	JSFX_ASSETS
		"${CMAKE_CURRENT_SOURCE_DIR}/jsfx/lib"
		"${CMAKE_CURRENT_SOURCE_DIR}/jsfx/Data"
	IS_SYNTH
	NEEDS_MIDI_INPUT
	NEEDS_MIDI_OUTPUT
)

juce_add_console_app(midi_splash_smoke PRODUCT_NAME "midi_splash_smoke")

target_sources(midi_splash_smoke
	PRIVATE
		tools/midi_splash_smoke.cpp
)

target_include_directories(midi_splash_smoke
	PRIVATE
		"${mrgee_fx_SOURCE_DIR}/src"
)

target_compile_definitions(midi_splash_smoke
	PRIVATE
		JUCE_WEB_BROWSER=0
		JUCE_USE_CURL=0
)

target_link_libraries(midi_splash_smoke
	PRIVATE
		MidiSpash
		juce::juce_audio_utils
		juce::juce_dsp
	PUBLIC
		juce::juce_recommended_config_flags
		juce::juce_recommended_lto_flags
		juce::juce_recommended_warning_flags
)
