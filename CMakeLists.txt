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
    # License
    Source/license/LicenseManager.cpp
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
    Source/gui/VisceraLookAndFeel.cpp
    Source/gui/TabbedEffectSection.cpp
    Source/gui/HarmonicEditor.cpp
)

juce_add_binary_data(VisceraData SOURCES
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
    presets/Drums_FM_Kick.visc
    presets/Drums_Glide_Kick.visc
    presets/Drums_Microwave_Kick.visc
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
    presets/Drums_Glitch_Perc.visc
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
    # New factory presets - Glitch Perc (Drums)
    presets/Drums_Bit_Snap.visc
    presets/Drums_Zap_Hit.visc
    presets/Drums_Crackle.visc
    presets/Drums_Digi_Rim.visc
    presets/Drums_Glitch_Pop.visc
    presets/Drums_Tin_Can.visc
    presets/Drums_Static_Burst.visc
    presets/Drums_Micro_Blip.visc
    presets/Drums_Hex_Drum.visc
    presets/Drums_Click_Dust.visc
    # New factory presets - Hyperpop Plucks (Lead)
    presets/Lead_Bubble_Pop.visc
    presets/Lead_Crystal_Tap.visc
    presets/Lead_Sugar_Pluck.visc
    presets/Lead_Neon_Stab.visc
    presets/Lead_Pixel_Chime.visc
    # New factory presets - Dubstep Bass
    presets/Bass_Saw_Tear.visc
    presets/Bass_Gutter_Growl.visc
    presets/Bass_Razor_Sub.visc
    presets/Bass_Neuro_Bite.visc
    presets/Bass_XOR_Stomp.visc
    presets/Bass_Wub_Cannon.visc
    presets/Bass_Filth_Stab.visc
    presets/Bass_Steel_Jaw.visc
    presets/Bass_Acid_Scream.visc
    presets/Bass_Crunch_Drop.visc
    presets/Bass_Void_Punch.visc
    # New factory presets - DnB Jump Up / Belgian DnB
    presets/Bass_Frog_Ribbit.visc
    presets/Bass_Jump_Razor.visc
    presets/Bass_Foghorn_Stab.visc
    # New factory presets - Trance / Eurodance
    presets/Lead_Fairy_Dust.visc
    presets/Lead_Euro_Dream.visc
    presets/Lead_Cotton_Candy.visc
    # New factory presets - Heaven Plucks
    presets/Lead_Angel_Pluck.visc
    presets/Lead_Cloud_Nine.visc
    presets/Lead_Halo_Stab.visc
    # New factory presets - Psy Trance
    presets/Bass_Psy_Squelch.visc
    presets/Bass_Goa_Acid.visc
    presets/Bass_Third_Eye.visc
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
        "-framework IOKit"
        "-framework Security"
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

# =============================================================================
# Test suite (opt-in: cmake -DVISCERA_BUILD_TESTS=ON)
# =============================================================================
option(VISCERA_BUILD_TESTS "Build the Viscera test suite" OFF)

if(VISCERA_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.2
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(VisceraTests
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
    )

    target_include_directories(VisceraTests PRIVATE Source tests)

    target_link_libraries(VisceraTests PRIVATE
        Catch2::Catch2
        VisceraData
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_events
    )

    target_compile_definitions(VisceraTests PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        VISCERA_HEADLESS_TESTS=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        # Plugin identity macros (normally generated by juce_add_plugin)
        JucePlugin_Name="Viscera"
        JucePlugin_ManufacturerCode=0x42625379
        JucePlugin_PluginCode=0x426c426b
        JucePlugin_IsSynth=1
        JucePlugin_WantsMidiInput=1
        JucePlugin_ProducesMidiOutput=0
        JucePlugin_IsMidiEffect=0
        JucePlugin_EditorRequiresKeyboardFocus=1
        JucePlugin_Manufacturer="Viscera"
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
        JucePlugin_AUExportPrefix=VisceraAU
        JucePlugin_AUExportPrefixQuoted="VisceraAU"
        JucePlugin_AUManufacturerCode=JucePlugin_ManufacturerCode
        JucePlugin_CFBundleIdentifier=com.viscera.viscera
        JucePlugin_AAXIdentifier=com.viscera.viscera
        JucePlugin_AAXManufacturerCode=JucePlugin_ManufacturerCode
        JucePlugin_AAXProductId=JucePlugin_PluginCode
        JucePlugin_AAXCategory=0
        JucePlugin_AAXDisableBypass=0
        JucePlugin_AAXDisableMultiMono=0
        JucePlugin_MaxNumInputChannels=0
        JucePlugin_MaxNumOutputChannels=2
        JucePlugin_PreferredChannelConfigurations={0,2}
    )

    target_compile_features(VisceraTests PRIVATE cxx_std_17)
endif()
