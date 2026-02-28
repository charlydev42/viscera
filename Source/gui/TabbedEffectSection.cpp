// TabbedEffectSection.cpp — Tabbed, stacked, or grid container for Delay/Reverb/Liquid/Rubber
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
        tabButtons[i].setPaintingIsUnclipped(true);
        tabButtons[i].onClick = [this, i] { switchTab(i); };
        addAndMakeVisible(tabButtons[i]);
    }

    addAndMakeVisible(delaySection);
    addAndMakeVisible(reverbSection);
    addAndMakeVisible(liquidSection);
    addAndMakeVisible(rubberSection);

    switchTab(0);
}

void TabbedEffectSection::setLayout(Layout layout)
{
    if (currentLayout == layout) return;
    currentLayout = layout;
    stackedMode = (layout == Stacked);

    for (int i = 0; i < 4; ++i)
        tabButtons[i].setVisible(layout == Tabbed);

    if (layout == Tabbed)
    {
        switchTab(activeTab);
    }
    else
    {
        delaySection.setVisible(true);
        reverbSection.setVisible(true);
        liquidSection.setVisible(true);
        rubberSection.setVisible(true);
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
    }

    delaySection.setVisible(activeTab == 0);
    reverbSection.setVisible(activeTab == 1);
    liquidSection.setVisible(activeTab == 2);
    rubberSection.setVisible(activeTab == 3);
}

void TabbedEffectSection::paint(juce::Graphics& g)
{
    if (currentLayout == Tabbed) return;

    static const char* names[] = { "Delay", "Reverb", "Liquid", "Rubber" };
    int headerH = 12;

    for (int i = 0; i < 4; ++i)
    {
        auto pb = panelBounds[i];

        // Section label
        g.setColour(juce::Colour(VisceraLookAndFeel::kTextColor));
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
        g.drawText(names[i], pb.getX(), pb.getY(), pb.getWidth(), headerH,
                   juce::Justification::centred);

        // Thin separator line between effects (not above the first one)
        if (i > 0)
        {
            float y = (float) pb.getY() - 1.0f;
            g.setColour(juce::Colour(VisceraLookAndFeel::kShadowDark).withAlpha(0.25f));
            g.drawHorizontalLine((int) y, (float) pb.getX() + 6, (float) pb.getRight() - 6);
        }
    }
}

void TabbedEffectSection::resized()
{
    auto area = getLocalBounds();
    juce::Component* sections[] = { &delaySection, &reverbSection, &liquidSection, &rubberSection };

    if (currentLayout == Stacked)
    {
        int gap = 1;
        int headerH = 12;
        int panelH = (area.getHeight() - gap * 3) / 4;

        for (int i = 0; i < 4; ++i)
        {
            auto panel = area.removeFromTop(panelH);
            panelBounds[i] = panel;
            sections[i]->setBounds(panel.withTrimmedTop(headerH));
            if (i < 3) area.removeFromTop(gap);
        }
    }
    else if (currentLayout == Grid)
    {
        // 2x2 grid: top row [Delay | Reverb], bottom row [Liquid | Rubber]
        int headerH = 12;
        int gap = 3;
        int rowH = (area.getHeight() - gap) / 2;
        int colW = (area.getWidth() - gap) / 2;

        for (int i = 0; i < 4; ++i)
        {
            int col = i % 2;
            int row = i / 2;
            auto panel = juce::Rectangle<int>(
                area.getX() + col * (colW + gap),
                area.getY() + row * (rowH + gap),
                colW, rowH);
            panelBounds[i] = panel;
            sections[i]->setBounds(panel.withTrimmedTop(headerH).reduced(2, 0));
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
