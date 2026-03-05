// TestMain.cpp — Catch2 entry point with JUCE MessageManager init
// MessageManager is required for AudioProcessor construction (APVTS, timers)
#include <catch2/catch_session.hpp>
#include <juce_events/juce_events.h>

int main(int argc, char* argv[])
{
    // JUCE needs a MessageManager for AudioProcessor and ValueTree
    juce::MessageManager::getInstance();

    int result = Catch::Session().run(argc, argv);

    juce::MessageManager::deleteInstance();
    return result;
}
