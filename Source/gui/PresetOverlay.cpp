// PresetOverlay.cpp — Inline neumorphic grid preset browser
#include "PresetOverlay.h"
#include "ParasiteLookAndFeel.h"
#include "../PluginProcessor.h"
#include <algorithm>
#include <vector>

static const juce::StringArray kCategories { "All", "Bass", "Lead", "Pluck", "Keys", "Pad", "Texture", "Drums", "FX" };

// Draw a heart shape centered in a rectangle using vector paths
static void drawHeart(juce::Graphics& g, juce::Rectangle<float> area, bool filled)
{
    float cx = area.getCentreX();
    float cy = area.getCentreY();
    float w = area.getWidth() * 0.45f;
    float h = area.getHeight() * 0.45f;

    juce::Path heart;
    heart.startNewSubPath(cx, cy + h * 0.8f);
    heart.cubicTo(cx - w * 1.5f, cy - h * 0.2f,
                  cx - w * 0.8f, cy - h * 1.2f,
                  cx, cy - h * 0.4f);
    heart.cubicTo(cx + w * 0.8f, cy - h * 1.2f,
                  cx + w * 1.5f, cy - h * 0.2f,
                  cx, cy + h * 0.8f);
    heart.closeSubPath();

    if (filled)
        g.fillPath(heart);
    else
        g.strokePath(heart, juce::PathStrokeType(1.2f));
}

PresetOverlay::PresetOverlay(ParasiteProcessor& processor)
    : proc(processor)
{
    setWantsKeyboardFocus(false);

    // Pack selector dropdown
    packSelector.setMouseClickGrabsKeyboardFocus(false);
    packSelector.onChange = [this] {
        selectedPack = packSelector.getText();
        scrollOffset = 0;
        confirmDeleteCard = -1;
        rebuildCards();
        focusedCard = -1;
        repaint();
    };
    addAndMakeVisible(packSelector);

    for (int i = 0; i < 9; ++i)
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
        proc.sendPreviewNoteOff();
        noteIsOn = false;
        stopTimer();
    }
}

void PresetOverlay::refresh()
{
    proc.buildPresetRegistry();

    // Rebuild pack dropdown
    auto packs = proc.getAvailablePacks();
    packSelector.clear(juce::dontSendNotification);
    for (int i = 0; i < packs.size(); ++i)
        packSelector.addItem(packs[i], i + 1);
    int idx = packs.indexOf(selectedPack);
    packSelector.setSelectedItemIndex(idx >= 0 ? idx : 0, juce::dontSendNotification);

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

    bool isCategoryTab = (selectedCategory != "All");

    // Lambda to check if a preset passes the fav filter
    auto passesFavFilter = [&](int i) {
        if (!favFilterOn) return true;
        return proc.isFavorite(registry[static_cast<size_t>(i)].name);
    };

    // Lambda to check if a preset passes the pack filter
    auto passesPackFilter = [&](int i) {
        if (selectedPack == "All") return true;
        return registry[static_cast<size_t>(i)].pack == selectedPack;
    };

    auto passesFilters = [&](int i) {
        return passesFavFilter(i) && passesPackFilter(i);
    };

    if (isCategoryTab)
    {
        // Factory presets in this category, sorted alphabetically
        std::vector<int> factoryIndices;
        for (int i = 0; i < static_cast<int>(registry.size()); ++i)
        {
            auto& entry = registry[static_cast<size_t>(i)];
            if (entry.isFactory && entry.category != "Init" && entry.category == selectedCategory && passesFilters(i))
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
            if (!entry.isFactory && entry.category == selectedCategory && passesFilters(i))
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
        // "All" — collect, sort, add
        std::vector<int> indices;
        for (int i = 0; i < static_cast<int>(registry.size()); ++i)
        {
            auto& entry = registry[static_cast<size_t>(i)];
            if (entry.isFactory && entry.category == "Init") continue;
            if (!passesFilters(i)) continue;
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
        proc.sendPreviewNoteOff();
        stopTimer();
    }
    proc.sendPreviewNoteOn();
    noteIsOn = true;
    startTimer(350); // note-off after 350ms
}

void PresetOverlay::timerCallback()
{
    if (noteIsOn)
    {
        proc.sendPreviewNoteOff();
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

juce::Rectangle<int> PresetOverlay::favHeartBounds(int cardIndex) const
{
    if (cardIndex < 0 || cardIndex >= static_cast<int>(cards.size()))
        return {};
    auto& card = cards[static_cast<size_t>(cardIndex)];
    return { card.bounds.getRight() - 20, card.bounds.getBottom() - 18, 16, 16 };
}

void PresetOverlay::mouseDown(const juce::MouseEvent& e)
{
    auto pt = e.getPosition();

    // Heart toggle button
    if (favToggleBounds.contains(pt))
    {
        favFilterOn = !favFilterOn;
        scrollOffset = 0;
        confirmDeleteCard = -1;
        rebuildCards();
        focusedCard = -1;
        repaint();
        return;
    }

    // If in delete confirmation, handle on mouseDown
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
            confirmDeleteCard = -1;
            repaint();
            return;
        }
        confirmDeleteCard = -1;
        repaint();
    }

    // Instant card selection on mouseDown (like Ableton/Serum browser)
    int idx = cardAtPoint(pt);
    if (idx >= 0)
    {
        auto& reg = proc.getPresetRegistry();
        int ri = cards[static_cast<size_t>(idx)].registryIndex;
        if (ri < 0 || ri >= static_cast<int>(reg.size())) return;
        auto& entry = reg[static_cast<size_t>(ri)];

        // Check delete cross first
        if (!entry.isFactory && deleteXBounds(idx).contains(pt))
        {
            confirmDeleteCard = idx;
            repaint();
            return;
        }

        // Check favorite heart
        if (favHeartBounds(idx).contains(pt))
        {
            proc.toggleFavorite(entry.name);
            if (favFilterOn)
                rebuildCards();
            repaint();
            return;
        }

        // Single-click: load + preview, browser stays open (audition mode)
        // Double-click closes the browser (commit).
        selectCard(idx);
    }
}

void PresetOverlay::mouseUp(const juce::MouseEvent&)
{
    // Selection handled in mouseDown for instant feedback
}

void PresetOverlay::mouseDoubleClick(const juce::MouseEvent& e)
{
    int idx = cardAtPoint(e.getPosition());
    if (idx >= 0)
    {
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

    // Pack selector dropdown on the left
    int dropW = 90;
    packSelector.setBounds(pillRow.removeFromLeft(dropW));
    pillRow.removeFromLeft(6);

    // Heart toggle on the right side of the pill row (same height as pills)
    int heartW = pillH;
    auto heartArea = pillRow.removeFromRight(heartW);
    favToggleBounds = heartArea;
    pillRow.removeFromRight(4);

    // 9 categories (All + 8 real) with 8 gaps of 4px between them.
    // Previous formula assumed 7 pills which clipped Drums + FX off-screen.
    constexpr int kNumPills = 9;
    constexpr int kGap = 4;
    const int pillW = (pillRow.getWidth() - (kNumPills - 1) * kGap) / kNumPills;
    for (int i = 0; i < kNumPills; ++i)
    {
        categoryButtons[i].setBounds(pillRow.removeFromLeft(pillW));
        if (i < kNumPills - 1)
            pillRow.removeFromLeft(kGap);
    }

    rebuildCards();
}

void PresetOverlay::paint(juce::Graphics& g)
{
    // Fill background
    g.fillAll(juce::Colour(ParasiteLookAndFeel::kBgColor));

    auto& registry = proc.getPresetRegistry();
    int currentIdx = proc.getCurrentPresetIndex();

    auto monoFont = juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain);
    auto smallFont = juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain);

    auto accentCol = juce::Colour(ParasiteLookAndFeel::kAccentColor);

    for (int i = 0; i < 9; ++i)
    {
        bool sel = (kCategories[i] == selectedCategory);
        categoryButtons[i].setColour(juce::TextButton::textColourOnId,
            sel ? accentCol : juce::Colour(ParasiteLookAndFeel::kTextColor));
        categoryButtons[i].setColour(juce::TextButton::textColourOffId,
            sel ? accentCol : juce::Colour(ParasiteLookAndFeel::kTextColor));
    }

    // Draw heart toggle button (pill style, matching category tabs)
    {
        auto hb = favToggleBounds.toFloat().reduced(1.0f);
        float cr = hb.getHeight() * 0.5f;

        if (favFilterOn)
        {
            // Pressed inset style (subtle for small button)
            g.setColour(juce::Colour(ParasiteLookAndFeel::kBgColor).darker(0.03f));
            g.fillRoundedRectangle(hb, cr);
            g.saveState();
            juce::Path clip;
            clip.addRoundedRectangle(hb, cr);
            g.reduceClipRegion(clip);
            juce::DropShadow darkIn(juce::Colour(ParasiteLookAndFeel::kShadowDark).withAlpha(0.25f), 2, { 1, 1 });
            darkIn.drawForRectangle(g, hb.toNearestInt());
            juce::DropShadow lightIn(juce::Colour(ParasiteLookAndFeel::kShadowLight).withAlpha(0.25f), 2, { -1, -1 });
            lightIn.drawForRectangle(g, hb.toNearestInt());
            g.restoreState();
        }
        else
        {
            // Raised pill style
            juce::Path pillPath;
            pillPath.addRoundedRectangle(hb, cr);
            juce::DropShadow lightSh(juce::Colour(ParasiteLookAndFeel::kShadowLight).withAlpha(0.5f), 2, { -1, -1 });
            lightSh.drawForPath(g, pillPath);
            juce::DropShadow darkSh(juce::Colour(ParasiteLookAndFeel::kShadowDark).withAlpha(0.4f), 2, { 1, 1 });
            darkSh.drawForPath(g, pillPath);
            g.setColour(juce::Colour(ParasiteLookAndFeel::kBgColor));
            g.fillRoundedRectangle(hb, cr);
        }

        auto heartRect = hb.reduced(4.0f);
        if (favFilterOn)
            g.setColour(accentCol);
        else
            g.setColour(juce::Colour(ParasiteLookAndFeel::kTextColor).withAlpha(0.4f));
        drawHeart(g, heartRect, favFilterOn);
    }

    // Clip to grid area (below pills, with shadow padding above)
    auto clipRect = getLocalBounds().withTop(gridTop - 4);
    g.saveState();
    g.reduceClipRegion(clipRect);

    auto accent = juce::Colour(ParasiteLookAndFeel::kAccentColor);

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
            auto lineCol = juce::Colour(ParasiteLookAndFeel::kShadowDark).withAlpha(ParasiteLookAndFeel::darkMode ? 0.4f : 0.18f);

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
            g.setColour(juce::Colour(ParasiteLookAndFeel::kTextColor).withAlpha(0.45f));
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
        ParasiteLookAndFeel::drawNeumorphicRect(g, bf, 8.0f, false);

        // Hover: subtle brighten
        if (isHovered && !isActive)
        {
            g.setColour(juce::Colour(ParasiteLookAndFeel::kShadowLight).withAlpha(0.25f));
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

        if (card.registryIndex < 0 || card.registryIndex >= static_cast<int>(registry.size()))
            continue;
        auto& entry = registry[static_cast<size_t>(card.registryIndex)];

        // Delete confirmation: replace card content entirely
        if (i == confirmDeleteCard)
        {
            // Opaque background over the neumorphic card
            g.setColour(juce::Colour(ParasiteLookAndFeel::kBgColor));
            g.fillRoundedRectangle(bf, 8.0f);

            g.setColour(juce::Colour(ParasiteLookAndFeel::kTextColor));
            g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain));
            auto topHalf = card.bounds.withTrimmedBottom(card.bounds.getHeight() / 2);
            g.drawText("Delete this preset?", topHalf, juce::Justification::centred);

            auto botHalf = card.bounds.withTrimmedTop(card.bounds.getHeight() / 2);
            auto yesArea = botHalf.removeFromLeft(botHalf.getWidth() / 2);
            auto noArea = botHalf;

            bool hoverYes = yesArea.contains(getMouseXYRelative());
            bool hoverNo = noArea.contains(getMouseXYRelative());

            g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::bold));

            g.setColour(hoverYes ? accent : juce::Colour(ParasiteLookAndFeel::kTextColor).withAlpha(0.7f));
            g.drawText("Yes", yesArea, juce::Justification::centred);

            g.setColour(hoverNo ? accent : juce::Colour(ParasiteLookAndFeel::kTextColor).withAlpha(0.7f));
            g.drawText("No", noArea, juce::Justification::centred);
            continue; // skip normal card content
        }

        // Preset name
        g.setColour(isActive ? accentCol.darker(0.15f)
                             : juce::Colour(ParasiteLookAndFeel::kTextColor));
        g.setFont(monoFont);

        // Show category subtitle in All, User, and for user presets in category tabs
        bool showSubtitle = (selectedCategory == "All")
                         || (!entry.isFactory);

        if (showSubtitle)
        {
            auto nameArea = card.bounds.withTrimmedBottom(14);
            auto catArea = card.bounds.withTrimmedTop(card.bounds.getHeight() - 14);
            g.drawText(entry.name, nameArea, juce::Justification::centred);
            g.setFont(smallFont);
            g.setColour(juce::Colour(ParasiteLookAndFeel::kTextColor).withAlpha(0.5f));
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
            g.setColour(juce::Colour(ParasiteLookAndFeel::kTextColor).withAlpha(hoverX ? 0.8f : 0.3f));
            g.setFont(juce::Font(10.0f));
            g.drawText(juce::String::charToString(0x2715), xr, juce::Justification::centred);
        }

        // Favorite heart (bottom-right corner)
        {
            auto hr = favHeartBounds(i);
            bool isFav = proc.isFavorite(entry.name);
            bool hoverH = (i == hoveredCard) && hr.contains(getMouseXYRelative());
            if (isFav)
                g.setColour(accentCol.withAlpha(hoverH ? 1.0f : 0.8f));
            else
                g.setColour(juce::Colour(ParasiteLookAndFeel::kTextColor).withAlpha(hoverH ? 0.5f : 0.15f));
            drawHeart(g, hr.toFloat(), isFav);
        }
    }

    g.restoreState();
}
