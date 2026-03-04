// PresetOverlay.cpp — Inline neumorphic grid preset browser
#include "PresetOverlay.h"
#include "VisceraLookAndFeel.h"
#include "../PluginProcessor.h"
#include <algorithm>
#include <vector>

static const juce::StringArray kCategories { "All", "Bass", "Lead", "Pad", "FX", "Drums", "Texture", "User" };

PresetOverlay::PresetOverlay(VisceraProcessor& processor)
    : proc(processor)
{
    setWantsKeyboardFocus(true);

    for (int i = 0; i < 8; ++i)
    {
        categoryButtons[i].setButtonText(kCategories[i]);
        categoryButtons[i].setMouseClickGrabsKeyboardFocus(false);
        categoryButtons[i].onClick = [this, i] {
            selectedCategory = kCategories[i];
            scrollOffset = 0;
            confirmDeleteCard = -1;
            rebuildCards();
            focusedCard = -1;
            repaint();
        };
        addAndMakeVisible(categoryButtons[i]);
    }
}

void PresetOverlay::stopPreviewNote()
{
    if (noteIsOn)
    {
        proc.keyboardState.noteOff(1, 60, 0.0f);
        noteIsOn = false;
        stopTimer();
    }
}

void PresetOverlay::refresh()
{
    scrollOffset = 0;
    confirmDeleteCard = -1;
    savedPresetIndex = proc.getCurrentPresetIndex();
    rebuildCards();

    // Focus the currently active preset
    int currentIdx = proc.getCurrentPresetIndex();
    focusedCard = -1;
    for (int i = 0; i < static_cast<int>(cards.size()); ++i)
    {
        if (cards[static_cast<size_t>(i)].registryIndex == currentIdx)
        {
            focusedCard = i;
            break;
        }
    }

    grabKeyboardFocus();
    if (focusedCard >= 0)
        ensureCardVisible(focusedCard);

    repaint();
}

void PresetOverlay::rebuildCards()
{
    cards.clear();

    auto& registry = proc.getPresetRegistry();
    auto area = getLocalBounds();
    if (area.isEmpty()) return;

    int pillH = 30;
    auto grid = area.withTrimmedTop(pillH + 6).reduced(8, 0);
    gridTop = grid.getY();

    int cardW = (grid.getWidth() - (numCols - 1) * 8) / numCols;
    int cardH = 48;
    int gap = 8;
    int sepH = 6;

    int col = 0;
    int x = grid.getX();
    int y = gridTop - scrollOffset;

    auto addCard = [&](int i) {
        cards.push_back({ { x, y, cardW, cardH }, i, false });
        col++;
        if (col >= numCols)
        {
            col = 0;
            x = grid.getX();
            y += cardH + gap;
        }
        else
        {
            x += cardW + gap;
        }
    };

    auto addSeparator = [&]() {
        // Finish current row if partially filled
        if (col > 0)
        {
            col = 0;
            x = grid.getX();
            y += cardH + gap;
        }
        // Insert separator line
        cards.push_back({ { grid.getX(), y, grid.getWidth(), sepH }, -1, true });
        y += sepH + gap;
    };

    // Helper: sort indices alphabetically by preset name
    auto sortByName = [&](std::vector<int>& indices) {
        std::sort(indices.begin(), indices.end(), [&](int a, int b) {
            return registry[static_cast<size_t>(a)].name.compareIgnoreCase(
                   registry[static_cast<size_t>(b)].name) < 0;
        });
    };

    bool isCategoryTab = (selectedCategory != "All" && selectedCategory != "User");

    if (isCategoryTab)
    {
        // Factory presets in this category, sorted alphabetically
        std::vector<int> factoryIndices;
        for (int i = 0; i < static_cast<int>(registry.size()); ++i)
        {
            auto& entry = registry[static_cast<size_t>(i)];
            if (entry.isFactory && entry.category != "Init" && entry.category == selectedCategory)
                factoryIndices.push_back(i);
        }
        sortByName(factoryIndices);
        for (int i : factoryIndices)
            addCard(i);

        // User presets in this category, sorted alphabetically
        std::vector<int> userIndices;
        for (int i = 0; i < static_cast<int>(registry.size()); ++i)
        {
            auto& entry = registry[static_cast<size_t>(i)];
            if (!entry.isFactory && entry.category == selectedCategory)
                userIndices.push_back(i);
        }
        if (!userIndices.empty())
        {
            sortByName(userIndices);
            addSeparator();
            for (int i : userIndices)
                addCard(i);
        }
    }
    else
    {
        // "All" or "User" — collect, sort, add
        std::vector<int> indices;
        for (int i = 0; i < static_cast<int>(registry.size()); ++i)
        {
            auto& entry = registry[static_cast<size_t>(i)];
            if (entry.isFactory && entry.category == "Init") continue;
            if (selectedCategory == "User" && entry.isFactory) continue;
            indices.push_back(i);
        }
        sortByName(indices);
        for (int i : indices)
            addCard(i);
    }

    // Compute total content height
    if (!cards.empty())
    {
        auto& last = cards.back();
        totalContentHeight = (last.bounds.getBottom() + scrollOffset) - gridTop;
    }
    else
    {
        totalContentHeight = 0;
    }
}

int PresetOverlay::cardAtPoint(juce::Point<int> pt) const
{
    for (int i = 0; i < static_cast<int>(cards.size()); ++i)
        if (!cards[static_cast<size_t>(i)].isSeparator && cards[static_cast<size_t>(i)].bounds.contains(pt))
            return i;
    return -1;
}

void PresetOverlay::selectCard(int cardIndex)
{
    if (cardIndex < 0 || cardIndex >= static_cast<int>(cards.size()))
        return;

    focusedCard = cardIndex;
    proc.loadPresetAt(cards[static_cast<size_t>(cardIndex)].registryIndex);
    if (onPresetChanged) onPresetChanged();
    triggerPreviewNote();
    ensureCardVisible(cardIndex);
    repaint();
}

void PresetOverlay::triggerPreviewNote()
{
    // Send a short MIDI note to preview the sound
    if (noteIsOn)
    {
        proc.keyboardState.noteOff(1, 60, 0.0f);
        stopTimer();
    }
    proc.keyboardState.noteOn(1, 60, 0.7f);
    noteIsOn = true;
    startTimer(350); // note-off after 350ms
}

void PresetOverlay::timerCallback()
{
    if (noteIsOn)
    {
        proc.keyboardState.noteOff(1, 60, 0.0f);
        noteIsOn = false;
    }
    stopTimer();
}

void PresetOverlay::ensureCardVisible(int cardIndex)
{
    if (cardIndex < 0 || cardIndex >= static_cast<int>(cards.size()))
        return;

    auto& card = cards[static_cast<size_t>(cardIndex)];
    int visibleBottom = getHeight();

    if (card.bounds.getY() < gridTop)
    {
        scrollOffset -= (gridTop - card.bounds.getY());
        rebuildCards();
    }
    else if (card.bounds.getBottom() > visibleBottom)
    {
        scrollOffset += (card.bounds.getBottom() - visibleBottom);
        rebuildCards();
    }
}

void PresetOverlay::mouseMove(const juce::MouseEvent& e)
{
    int newHover = cardAtPoint(e.getPosition());
    if (newHover != hoveredCard)
    {
        hoveredCard = newHover;
        repaint();
    }
}

juce::Rectangle<int> PresetOverlay::deleteXBounds(int cardIndex) const
{
    if (cardIndex < 0 || cardIndex >= static_cast<int>(cards.size()))
        return {};
    auto& card = cards[static_cast<size_t>(cardIndex)];
    return { card.bounds.getX() + 4, card.bounds.getY() + 4, 16, 16 };
}

void PresetOverlay::mouseUp(const juce::MouseEvent& e)
{
    auto pt = e.getPosition();

    // Handle delete confirmation clicks
    if (confirmDeleteCard >= 0 && confirmDeleteCard < static_cast<int>(cards.size()))
    {
        auto& card = cards[static_cast<size_t>(confirmDeleteCard)];
        if (card.bounds.contains(pt))
        {
            auto botHalf = card.bounds.withTrimmedTop(card.bounds.getHeight() / 2);
            auto yesArea = botHalf.removeFromLeft(botHalf.getWidth() / 2);
            auto noArea = botHalf;

            if (yesArea.contains(pt))
            {
                // Delete the preset
                auto& registry = proc.getPresetRegistry();
                auto& entry = registry[static_cast<size_t>(card.registryIndex)];
                proc.deleteUserPreset(entry.userFileName);
                proc.buildPresetRegistry();
                confirmDeleteCard = -1;
                rebuildCards();
                if (onPresetChanged) onPresetChanged();
                repaint();
                return;
            }
            if (noArea.contains(pt))
            {
                confirmDeleteCard = -1;
                repaint();
                return;
            }
            // Clicked elsewhere on the confirm card — cancel
            confirmDeleteCard = -1;
            repaint();
            return;
        }
        // Clicked outside the confirm card — cancel
        confirmDeleteCard = -1;
        repaint();
    }

    int idx = cardAtPoint(pt);
    if (idx >= 0)
    {
        // Check if click is on the delete cross
        auto& entry = proc.getPresetRegistry()[static_cast<size_t>(cards[static_cast<size_t>(idx)].registryIndex)];
        if (!entry.isFactory && deleteXBounds(idx).contains(pt))
        {
            confirmDeleteCard = idx;
            repaint();
            return;
        }
        selectCard(idx);
    }
}

void PresetOverlay::mouseDoubleClick(const juce::MouseEvent& e)
{
    int idx = cardAtPoint(e.getPosition());
    if (idx >= 0)
    {
        proc.loadPresetAt(cards[static_cast<size_t>(idx)].registryIndex);
        stopPreviewNote();
        if (onClose) onClose();
    }
}

void PresetOverlay::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    int visibleH = getHeight() - gridTop;
    int maxScroll = juce::jmax(0, totalContentHeight - visibleH);
    scrollOffset = juce::jlimit(0, maxScroll, scrollOffset - static_cast<int>(wheel.deltaY * 200.0f));
    rebuildCards();
    repaint();
}

bool PresetOverlay::keyPressed(const juce::KeyPress& key)
{
    int total = static_cast<int>(cards.size());
    if (total == 0) return false;

    int fc = juce::jmax(0, focusedCard);

    if (key == juce::KeyPress::rightKey)
    {
        selectCard(juce::jmin(fc + 1, total - 1));
        return true;
    }
    if (key == juce::KeyPress::leftKey)
    {
        selectCard(juce::jmax(fc - 1, 0));
        return true;
    }
    if (key == juce::KeyPress::downKey)
    {
        selectCard(juce::jmin(fc + numCols, total - 1));
        return true;
    }
    if (key == juce::KeyPress::upKey)
    {
        selectCard(juce::jmax(fc - numCols, 0));
        return true;
    }
    if (key == juce::KeyPress::returnKey)
    {
        // Confirm: keep current preset, close
        if (focusedCard >= 0)
            proc.loadPresetAt(cards[static_cast<size_t>(focusedCard)].registryIndex);
        stopPreviewNote();
        if (onClose) onClose();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        // Cancel: restore saved preset, close
        stopPreviewNote();
        if (savedPresetIndex >= 0)
            proc.loadPresetAt(savedPresetIndex);
        if (onCancel) onCancel();
        return true;
    }

    return false;
}

void PresetOverlay::resized()
{
    auto area = getLocalBounds().reduced(8, 4);
    int pillH = 26;
    auto pillRow = area.removeFromTop(pillH);

    int pillW = (pillRow.getWidth() - 7 * 4) / 8;
    for (int i = 0; i < 8; ++i)
    {
        categoryButtons[i].setBounds(pillRow.removeFromLeft(pillW));
        pillRow.removeFromLeft(4);
    }

    rebuildCards();
}

void PresetOverlay::paint(juce::Graphics& g)
{
    // Fill background
    g.fillAll(juce::Colour(VisceraLookAndFeel::kBgColor));

    auto& registry = proc.getPresetRegistry();
    int currentIdx = proc.getCurrentPresetIndex();

    auto monoFont = juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain);
    auto smallFont = juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain);

    // Style category pills
    for (int i = 0; i < 8; ++i)
    {
        bool sel = (kCategories[i] == selectedCategory);
        categoryButtons[i].setColour(juce::TextButton::textColourOnId,
            sel ? juce::Colour(VisceraLookAndFeel::kAccentColor)
                : juce::Colour(VisceraLookAndFeel::kTextColor));
        categoryButtons[i].setColour(juce::TextButton::textColourOffId,
            sel ? juce::Colour(VisceraLookAndFeel::kAccentColor)
                : juce::Colour(VisceraLookAndFeel::kTextColor));
    }

    // Clip to grid area (below pills)
    auto clipRect = getLocalBounds().withTop(gridTop);
    g.saveState();
    g.reduceClipRegion(clipRect);

    auto accent = juce::Colour(VisceraLookAndFeel::kAccentColor);

    // Draw cards and separators
    for (int i = 0; i < static_cast<int>(cards.size()); ++i)
    {
        auto& card = cards[static_cast<size_t>(i)];
        // Skip cards fully outside visible area
        if (card.bounds.getBottom() < gridTop || card.bounds.getY() > getHeight())
            continue;

        // Draw separator line with "User" label
        if (card.isSeparator)
        {
            auto sepBounds = card.bounds.toFloat();
            float lineY = sepBounds.getCentreY();
            auto lineCol = juce::Colour(VisceraLookAndFeel::kShadowDark).withAlpha(VisceraLookAndFeel::darkMode ? 0.4f : 0.18f);

            // Measure label
            auto labelFont = juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::bold);
            juce::String label = "User";
            float labelW = labelFont.getStringWidthFloat(label) + 12.0f;
            float labelX = sepBounds.getCentreX() - labelW * 0.5f;

            // Line left of label
            g.setColour(lineCol);
            g.fillRect(sepBounds.getX() + 8.0f, lineY, labelX - sepBounds.getX() - 12.0f, 1.0f);
            // Line right of label
            g.fillRect(labelX + labelW + 4.0f, lineY, sepBounds.getRight() - labelX - labelW - 12.0f, 1.0f);

            // Label text
            g.setColour(juce::Colour(VisceraLookAndFeel::kTextColor).withAlpha(0.45f));
            g.setFont(labelFont);
            g.drawText(label, juce::Rectangle<float>(labelX, sepBounds.getY(), labelW, sepBounds.getHeight()),
                       juce::Justification::centred);
            continue;
        }

        auto bf = card.bounds.toFloat();
        bool isActive = (card.registryIndex == currentIdx);
        bool isHovered = (i == hoveredCard);
        bool isFocused = (i == focusedCard);

        // Neumorphic card (always raised)
        VisceraLookAndFeel::drawNeumorphicRect(g, bf, 8.0f, false);

        // Hover: subtle brighten
        if (isHovered && !isActive)
        {
            g.setColour(juce::Colour(VisceraLookAndFeel::kShadowLight).withAlpha(0.25f));
            g.fillRoundedRectangle(bf, 8.0f);
        }

        // Active indicator: small glowing dot top-right corner
        if (isActive)
        {
            float dotX = bf.getRight() - 8.0f;
            float dotY = bf.getY() + 8.0f;

            // Outer glow
            g.setColour(accent.withAlpha(0.12f));
            g.fillEllipse(dotX - 6.0f, dotY - 6.0f, 12.0f, 12.0f);
            // Inner glow
            g.setColour(accent.withAlpha(0.3f));
            g.fillEllipse(dotX - 3.5f, dotY - 3.5f, 7.0f, 7.0f);
            // Core dot
            g.setColour(accent);
            g.fillEllipse(dotX - 2.0f, dotY - 2.0f, 4.0f, 4.0f);
        }

        // (no visible focus ring — active dot is enough)

        auto& entry = registry[static_cast<size_t>(card.registryIndex)];

        // Delete confirmation: replace card content entirely
        if (i == confirmDeleteCard)
        {
            // Opaque background over the neumorphic card
            g.setColour(juce::Colour(VisceraLookAndFeel::kBgColor));
            g.fillRoundedRectangle(bf, 8.0f);

            g.setColour(juce::Colour(VisceraLookAndFeel::kTextColor));
            g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
            auto topHalf = card.bounds.withTrimmedBottom(card.bounds.getHeight() / 2);
            g.drawText("Delete this preset?", topHalf, juce::Justification::centred);

            auto botHalf = card.bounds.withTrimmedTop(card.bounds.getHeight() / 2);
            auto yesArea = botHalf.removeFromLeft(botHalf.getWidth() / 2);
            auto noArea = botHalf;

            bool hoverYes = yesArea.contains(getMouseXYRelative());
            bool hoverNo = noArea.contains(getMouseXYRelative());

            g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::bold));

            g.setColour(hoverYes ? accent : juce::Colour(VisceraLookAndFeel::kTextColor).withAlpha(0.7f));
            g.drawText("Yes", yesArea, juce::Justification::centred);

            g.setColour(hoverNo ? accent : juce::Colour(VisceraLookAndFeel::kTextColor).withAlpha(0.7f));
            g.drawText("No", noArea, juce::Justification::centred);
            continue; // skip normal card content
        }

        // Preset name
        g.setColour(isActive ? accent.withAlpha(0.95f)
                             : juce::Colour(VisceraLookAndFeel::kTextColor));
        g.setFont(monoFont);

        // Show category subtitle in All, User, and for user presets in category tabs
        bool showSubtitle = (selectedCategory == "All")
                         || (selectedCategory == "User")
                         || (!entry.isFactory);

        if (showSubtitle)
        {
            auto nameArea = card.bounds.withTrimmedBottom(14);
            auto catArea = card.bounds.withTrimmedTop(card.bounds.getHeight() - 14);
            g.drawText(entry.name, nameArea, juce::Justification::centred);
            g.setFont(smallFont);
            g.setColour(juce::Colour(VisceraLookAndFeel::kTextColor).withAlpha(0.5f));
            g.drawText(entry.category, catArea, juce::Justification::centred);
        }
        else
        {
            g.drawText(entry.name, card.bounds, juce::Justification::centred);
        }

        // Delete cross for user presets (top-left corner)
        if (!entry.isFactory)
        {
            auto xr = deleteXBounds(i);
            bool hoverX = (i == hoveredCard) && xr.contains(getMouseXYRelative());
            g.setColour(juce::Colour(VisceraLookAndFeel::kTextColor).withAlpha(hoverX ? 0.8f : 0.3f));
            g.setFont(juce::Font(10.0f));
            g.drawText(juce::String::charToString(0x2715), xr, juce::Justification::centred);
        }
    }

    g.restoreState();
}
