// TabbedEffectSection.cpp — Tabbed or stacked container for Delay/Reverb/Liquid/Rubber
#include "TabbedEffectSection.h"
#include "VisceraLookAndFeel.h"

TabbedEffectSection::TabbedEffectSection(juce::AudioProcessorValueTreeState& apvts)
    : delaySection(apvts),
      reverbSection(apvts),
      liquidSection(apvts),
      rubberSection(apvts)
{
    static const char* tabNames[] = { "Delay", "Reverb", "Liquid", "Rubber" };
    for (int i = 0; i < 4; ++i)
    {
        tabButtons[i].setButtonText(tabNames[i]);
        tabButtons[i].setClickingTogglesState(false);
        tabButtons[i].onClick = [this, i] { switchTab(i); };
        addAndMakeVisible(tabButtons[i]);
    }

    addAndMakeVisible(delaySection);
    addAndMakeVisible(reverbSection);
    addAndMakeVisible(liquidSection);
    addAndMakeVisible(rubberSection);

    switchTab(0);
}

void TabbedEffectSection::setStacked(bool stacked)
{
    if (stackedMode == stacked) return;
    stackedMode = stacked;

    for (int i = 0; i < 4; ++i)
        tabButtons[i].setVisible(!stacked);

    if (stacked)
    {
        delaySection.setVisible(true);
        reverbSection.setVisible(true);
        liquidSection.setVisible(true);
        rubberSection.setVisible(true);
    }
    else
    {
        switchTab(activeTab);
    }

    resized();
    repaint();
}

void TabbedEffectSection::switchTab(int tab)
{
    activeTab = juce::jlimit(0, 3, tab);
    auto accentCol = juce::Colour(VisceraLookAndFeel::kAccentColor);

    for (int i = 0; i < 4; ++i)
    {
        bool active = (i == activeTab);
        tabButtons[i].setColour(juce::TextButton::buttonColourId,
            active ? accentCol.withAlpha(0.6f)
                   : juce::Colour(VisceraLookAndFeel::kPanelColor));
        tabButtons[i].setColour(juce::TextButton::textColourOffId,
            active ? juce::Colours::white
                   : juce::Colour(VisceraLookAndFeel::kTextColor));
    }

    delaySection.setVisible(activeTab == 0);
    reverbSection.setVisible(activeTab == 1);
    liquidSection.setVisible(activeTab == 2);
    rubberSection.setVisible(activeTab == 3);
}

void TabbedEffectSection::paint(juce::Graphics& g)
{
    if (!stackedMode) return;

    static const char* names[] = { "Delay", "Reverb", "Liquid", "Rubber" };
    int headerH = 14;

    for (int i = 0; i < 4; ++i)
    {
        auto pb = panelBounds[i];
        g.setColour(juce::Colour(VisceraLookAndFeel::kPanelColor).darker(0.15f));
        g.fillRoundedRectangle(pb.toFloat(), 3.0f);

        g.setColour(juce::Colour(VisceraLookAndFeel::kAccentColor).withAlpha(0.5f));
        g.fillRect(pb.getX() + 2, pb.getY(), pb.getWidth() - 4, 1);

        g.setColour(juce::Colour(VisceraLookAndFeel::kTextColor));
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
        g.drawText(names[i], pb.getX(), pb.getY() + 1, pb.getWidth(), headerH,
                   juce::Justification::centred);
    }
}

void TabbedEffectSection::resized()
{
    auto area = getLocalBounds();
    juce::Component* sections[] = { &delaySection, &reverbSection, &liquidSection, &rubberSection };

    if (stackedMode)
    {
        int gap = 3;
        int headerH = 14;
        int panelH = (area.getHeight() - gap * 3) / 4;

        for (int i = 0; i < 4; ++i)
        {
            auto panel = area.removeFromTop(panelH);
            panelBounds[i] = panel;
            sections[i]->setBounds(panel.withTrimmedTop(headerH).reduced(2, 0));
            if (i < 3) area.removeFromTop(gap);
        }
    }
    else
    {
        auto tabRow = area.removeFromTop(20);
        int tabW = tabRow.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
            tabButtons[i].setBounds(tabRow.removeFromLeft(tabW));

        for (int i = 0; i < 4; ++i)
            sections[i]->setBounds(area);
    }
}
