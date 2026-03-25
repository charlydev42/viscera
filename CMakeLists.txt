cmake_minimum_required(VERSION 3.22...3.31)
project(Parasite VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures")

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
juce_add_plugin(Parasite
    COMPANY_NAME            "Thunderdolphin"
    PLUGIN_MANUFACTURER_CODE BbSy
    PLUGIN_CODE              BlBk
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD  TRUE
    FORMATS                  VST3 AU Standalone
    PRODUCT_NAME             "Parasite"
    BUNDLE_ID                "com.thunderdolphin.parasite"
    AU_MAIN_TYPE             kAudioUnitType_MusicDevice
)

# --- Sources ---
target_sources(Parasite PRIVATE
    # Processor & Editor
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    # DSP
    Source/dsp/FMVoice.cpp
    # License + Cloud
    Source/license/LicenseManager.cpp
    Source/cloud/CloudPresetManager.cpp
    # GUI
    Source/gui/LicenseOverlay.cpp
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
    Source/gui/PresetOverlay.cpp
    Source/gui/SaveOverlay.cpp
    Source/gui/ParasiteLookAndFeel.cpp
    Source/gui/TabbedEffectSection.cpp
    Source/gui/HarmonicEditor.cpp
)

juce_add_binary_data(ParasiteData SOURCES
    graphics/parasite_logo_light.png
    graphics/parasite_logo_dark.png
    graphics/parasite_logo_light_nodolph.png
    graphics/parasite_logo_dark_nodolph.png
    graphics/parasite_logo_neutral.png
    graphics/parasite_logo_neutral_dark.png
    # Factory presets (.prst)
    presets/Init.prst
    presets/Bass_808_Sub.prst
    presets/Bass_Acid_Squelch.prst
    presets/Drums_FM_Kick.prst
    presets/Drums_Glide_Kick.prst
    presets/Drums_Microwave_Kick.prst
    presets/Bass_Pluck_Bass.prst
    presets/Bass_Reese.prst
    presets/Bass_Sub_Growl.prst
    presets/Bass_Tape_Wobble.prst
    presets/Bass_Wobble_Bass.prst
    presets/Lead_Acid_Lead.prst
    presets/Lead_Bright_Pluck.prst
    presets/Lead_Drift.prst
    presets/Lead_FM_Brass.prst
    presets/Lead_Glass.prst
    presets/Lead_Saw_Lead.prst
    presets/Lead_Screamer.prst
    presets/Lead_Sync_Lead.prst
    presets/Lead_Sync_Sweep.prst
    presets/Lead_Whistle.prst
    presets/Pad_Analog_Strings.prst
    presets/Pad_Choir.prst
    presets/Pad_Crystal.prst
    presets/Pad_Deep_Space.prst
    presets/Pad_Ethereal_Pad.prst
    presets/Pad_FM_Organ.prst
    presets/Pad_Ice_Glass.prst
    presets/Pad_Shimmer.prst
    presets/Pad_Soft_Pulse.prst
    presets/Pad_Warm_Cloud.prst
    presets/FX_Alien_FX.prst
    presets/FX_Chaos_Engine.prst
    presets/Drums_Glitch_Perc.prst
    presets/FX_Impact.prst
    presets/FX_Laser.prst
    presets/FX_Metal_Bell.prst
    presets/FX_Robot_Voice.prst
    presets/FX_Siren.prst
    presets/FX_Stutter.prst
    presets/FX_Wind.prst
    presets/Texture_Dark_Drone.prst
    presets/Texture_Digital_Harsh.prst
    presets/Texture_Fog.prst
    presets/Texture_Grain.prst
    presets/Texture_Hive.prst
    presets/Texture_Nasal_Drone.prst
    presets/Texture_Rust.prst
    presets/Texture_Soft_Texture.prst
    presets/Texture_Wire.prst
    # New factory presets - Glitch Perc (Drums)
    presets/Drums_Bit_Snap.prst
    presets/Drums_Zap_Hit.prst
    presets/Drums_Crackle.prst
    presets/Drums_Digi_Rim.prst
    presets/Drums_Glitch_Pop.prst
    presets/Drums_Tin_Can.prst
    presets/Drums_Static_Burst.prst
    presets/Drums_Micro_Blip.prst
    presets/Drums_Hex_Drum.prst
    presets/Drums_Click_Dust.prst
    # New factory presets - Hyperpop Plucks (Lead)
    presets/Lead_Bubble_Pop.prst
    presets/Lead_Crystal_Tap.prst
    presets/Lead_Sugar_Pluck.prst
    presets/Lead_Neon_Stab.prst
    presets/Lead_Pixel_Chime.prst
    # New factory presets - Dubstep Bass
    presets/Bass_Saw_Tear.prst
    presets/Bass_Gutter_Growl.prst
    presets/Bass_Razor_Sub.prst
    presets/Bass_Neuro_Bite.prst
    presets/Bass_XOR_Stomp.prst
    presets/Bass_Wub_Cannon.prst
    presets/Bass_Filth_Stab.prst
    presets/Bass_Steel_Jaw.prst
    presets/Bass_Acid_Scream.prst
    presets/Bass_Crunch_Drop.prst
    presets/Bass_Void_Punch.prst
    # New factory presets - DnB Jump Up / Belgian DnB
    presets/Bass_Frog_Ribbit.prst
    presets/Bass_Jump_Razor.prst
    presets/Bass_Foghorn_Stab.prst
    # New factory presets - Trance / Eurodance
    presets/Lead_Fairy_Dust.prst
    presets/Lead_Euro_Dream.prst
    presets/Lead_Cotton_Candy.prst
    # New factory presets - Heaven Plucks
    presets/Lead_Angel_Pluck.prst
    presets/Lead_Cloud_Nine.prst
    presets/Lead_Halo_Stab.prst
    # New factory presets - Psy Trance
    presets/Bass_Psy_Squelch.prst
    presets/Bass_Goa_Acid.prst
    presets/Bass_Third_Eye.prst
    # Vocal / Trance
    presets/Lead_Binary_Voice.prst
)

target_include_directories(Parasite PRIVATE Source)

# --- Modules JUCE nécessaires ---
target_link_libraries(Parasite
    PRIVATE
        ParasiteData
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_opengl
        "-framework IOKit"
        "-framework Security"
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# --- Définitions de compilation JUCE ---
target_compile_definitions(Parasite
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

# --- Security compiler flags ---
if(APPLE)
    target_compile_options(Parasite PRIVATE
        -fstack-protector-strong
    )
endif()

# --- Hardened Runtime (Xcode) ---
if(APPLE)
    set_target_properties(Parasite PROPERTIES
        XCODE_ATTRIBUTE_ENABLE_HARDENED_RUNTIME YES
    )
endif()

# =============================================================================
# Test suite (opt-in: cmake -DPARASITE_BUILD_TESTS=ON)
# =============================================================================
option(PARASITE_BUILD_TESTS "Build the Parasite test suite" OFF)

if(PARASITE_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.2
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(ParasiteTests
        tests/TestMain.cpp
        tests/test_Oscillator.cpp
        tests/test_ADSREnvelope.cpp
        tests/test_SVFilter.cpp
        tests/test_LFO.cpp
        tests/test_HarmonicTable.cpp
        tests/test_HemoFold.cpp
        tests/test_XORDistortion.cpp
        tests/test_DCBlocker.cpp
        tests/test_Effects.cpp
        tests/test_VolumeShaper.cpp
        tests/test_AllpassDisperser.cpp
        tests/test_FMVoice.cpp
        tests/test_Processor.cpp
        tests/test_Presets.cpp
        tests/test_StateRoundTrip.cpp
        tests/test_Stress.cpp
        # Source files needed (DSP + Processor, no GUI)
        Source/PluginProcessor.cpp
        Source/dsp/FMVoice.cpp
        Source/license/LicenseManager.cpp
        Source/cloud/CloudPresetManager.cpp
    )

    target_include_directories(ParasiteTests PRIVATE Source tests)

    target_link_libraries(ParasiteTests PRIVATE
        Catch2::Catch2
        ParasiteData
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_events
        "-framework IOKit"
        "-framework Security"
    )

    target_compile_definitions(ParasiteTests PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        PARASITE_HEADLESS_TESTS=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        # Plugin identity macros (normally generated by juce_add_plugin)
        JucePlugin_Name="Parasite"
        JucePlugin_ManufacturerCode=0x42625379
        JucePlugin_PluginCode=0x426c426b
        JucePlugin_IsSynth=1
        JucePlugin_WantsMidiInput=1
        JucePlugin_ProducesMidiOutput=0
        JucePlugin_IsMidiEffect=0
        JucePlugin_EditorRequiresKeyboardFocus=1
        JucePlugin_Manufacturer="Thunderdolphin"
        JucePlugin_ManufacturerWebsite=""
        JucePlugin_ManufacturerEmail=""
        JucePlugin_Desc="FM Synth"
        JucePlugin_VersionString="1.0.0"
        JucePlugin_VersionCode=0x10000
        JucePlugin_VSTUniqueID=JucePlugin_PluginCode
        JucePlugin_VSTCategory=kPlugCategSynth
        JucePlugin_Vst3Category="Instrument|Synth"
        JucePlugin_AUMainType=0x6175726d
        JucePlugin_AUSubType=JucePlugin_PluginCode
        JucePlugin_AUExportPrefix=ParasiteAU
        JucePlugin_AUExportPrefixQuoted="ParasiteAU"
        JucePlugin_AUManufacturerCode=JucePlugin_ManufacturerCode
        JucePlugin_CFBundleIdentifier=com.thunderdolphin.parasite
        JucePlugin_AAXIdentifier=com.thunderdolphin.parasite
        JucePlugin_AAXManufacturerCode=JucePlugin_ManufacturerCode
        JucePlugin_AAXProductId=JucePlugin_PluginCode
        JucePlugin_AAXCategory=0
        JucePlugin_AAXDisableBypass=0
        JucePlugin_AAXDisableMultiMono=0
        JucePlugin_MaxNumInputChannels=0
        JucePlugin_MaxNumOutputChannels=2
        JucePlugin_PreferredChannelConfigurations={0,2}
    )

    target_compile_features(ParasiteTests PRIVATE cxx_std_17)
endif()
