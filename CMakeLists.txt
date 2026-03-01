cmake_minimum_required(VERSION 3.22)
project(Viscera VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Télécharger JUCE via FetchContent ---
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# --- Définir le plugin ---
juce_add_plugin(Viscera
    COMPANY_NAME            "Viscera"
    PLUGIN_MANUFACTURER_CODE BbSy
    PLUGIN_CODE              BlBk
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD  TRUE
    FORMATS                  VST3 AU Standalone
    PRODUCT_NAME             "Viscera"
    AU_MAIN_TYPE             kAudioUnitType_MusicDevice
)

# --- Sources ---
target_sources(Viscera PRIVATE
    # Processor & Editor
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    # DSP
    Source/dsp/FMVoice.cpp
    # GUI
    Source/gui/LFOSection.cpp
    Source/gui/ModulatorSection.cpp
    Source/gui/CarrierSection.cpp
    Source/gui/ModMatrixSection.cpp
    Source/gui/FilterSection.cpp
    Source/gui/PitchEnvSection.cpp
    Source/gui/DelaySection.cpp
    Source/gui/ReverbSection.cpp
    Source/gui/LiquidSection.cpp
    Source/gui/RubberSection.cpp
    Source/gui/VolumeShaperSection.cpp
    Source/gui/VisualizerDisplay.cpp
    Source/gui/FlubberVisualizer.cpp
    Source/gui/GlobalSection.cpp
    Source/gui/PresetBrowser.cpp
    Source/gui/VisceraLookAndFeel.cpp
    Source/gui/TabbedEffectSection.cpp
)

juce_add_binary_data(VisceraData SOURCES
    logo.png
    "viscera logo fat.png"
    graphics/Knob_virgin.png
    graphics/Knob_empty_circle.png
    graphics/Knob_Blue.png
    graphics/Knob_circle_green.png
    graphics/viscera_logo_light.png
    graphics/viscera_logo_dark.png
    graphics/viscera_logo_light_nodolph.png
    graphics/viscera_logo_dark_nodolph.png
    graphics/viscera_logo_neutral.png
    graphics/viscera_logo_neutral_dark.png
    # Factory presets (.visc)
    presets/Init.visc
    presets/Bass_808_Sub.visc
    presets/Bass_Acid_Squelch.visc
    presets/Bass_FM_Kick.visc
    presets/Bass_Glide_Kick.visc
    presets/Bass_Microwave_Kick.visc
    presets/Bass_Pluck_Bass.visc
    presets/Bass_Reese.visc
    presets/Bass_Sub_Growl.visc
    presets/Bass_Tape_Wobble.visc
    presets/Bass_Wobble_Bass.visc
    presets/Lead_Acid_Lead.visc
    presets/Lead_Bright_Pluck.visc
    presets/Lead_Drift.visc
    presets/Lead_FM_Brass.visc
    presets/Lead_Glass.visc
    presets/Lead_Saw_Lead.visc
    presets/Lead_Screamer.visc
    presets/Lead_Sync_Lead.visc
    presets/Lead_Sync_Sweep.visc
    presets/Lead_Whistle.visc
    presets/Pad_Analog_Strings.visc
    presets/Pad_Choir.visc
    presets/Pad_Crystal.visc
    presets/Pad_Deep_Space.visc
    presets/Pad_Ethereal_Pad.visc
    presets/Pad_FM_Organ.visc
    presets/Pad_Ice_Glass.visc
    presets/Pad_Shimmer.visc
    presets/Pad_Soft_Pulse.visc
    presets/Pad_Warm_Cloud.visc
    presets/FX_Alien_FX.visc
    presets/FX_Chaos_Engine.visc
    presets/FX_Glitch_Perc.visc
    presets/FX_Impact.visc
    presets/FX_Laser.visc
    presets/FX_Metal_Bell.visc
    presets/FX_Robot_Voice.visc
    presets/FX_Siren.visc
    presets/FX_Stutter.visc
    presets/FX_Wind.visc
    presets/Texture_Dark_Drone.visc
    presets/Texture_Digital_Harsh.visc
    presets/Texture_Fog.visc
    presets/Texture_Grain.visc
    presets/Texture_Hive.visc
    presets/Texture_Nasal_Drone.visc
    presets/Texture_Rust.visc
    presets/Texture_Soft_Texture.visc
    presets/Texture_Wire.visc
)

target_include_directories(Viscera PRIVATE Source)

# --- Modules JUCE nécessaires ---
target_link_libraries(Viscera
    PRIVATE
        VisceraData
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# --- Définitions de compilation JUCE ---
target_compile_definitions(Viscera
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)
