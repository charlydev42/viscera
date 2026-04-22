// SnappedParameterBool.h — AudioParameterBool whose host-observable
// getValue() always returns exactly 0.0f or 1.0f.
//
// Stock juce::AudioParameterBool stores the raw float verbatim and
// getValue() returns that raw — so a mid-range float set via setValue()
// (as pluginval's fuzz test does) survives getStateInformation /
// setStateInformation round-trips and shows up as e.g. 0.304343. The
// typed `get()` accessor thresholds at 0.5, but `getValue()` doesn't.
//
// JUCE marks AudioParameterBool::setValue() private, so we can't wrap
// it from a subclass. getValue() is equally private in the derived class
// but virtual on the AudioProcessorParameter base — pluginval calls it
// via the base pointer, so overriding here takes effect. The result:
// the parameter stores raw as usual (automation works, listeners fire),
// but every host-observable read returns a clean 0.0f or 1.0f.
//
// Drop-in: same constructors, same IDs, same defaults, same range.
//
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class SnappedParameterBool : public juce::AudioParameterBool
{
public:
    using juce::AudioParameterBool::AudioParameterBool;

private:
    float getValue() const override
    {
        return get() ? 1.0f : 0.0f;
    }
};
