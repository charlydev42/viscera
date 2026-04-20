// SaveOverlay.cpp — Inline save-preset overlay
#include "SaveOverlay.h"
#include "ParasiteLookAndFeel.h"
#include "../PluginProcessor.h"

SaveOverlay::SaveOverlay(ParasiteProcessor& processor)
    : proc(processor)
{
    setWantsKeyboardFocus(false);

    nameEditor.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain));
    nameEditor.setJustification(juce::Justification::centredLeft);
    nameEditor.setIndents(12, 0);  // left padding for pill shape + vertical centering
    nameEditor.setBorder(juce::BorderSize<int>(0));
    nameEditor.setTextToShowWhenEmpty("Preset name...", juce::Colours::grey);
    nameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(ParasiteLookAndFeel::kDisplayBg));
    nameEditor.setColour(juce::TextEditor::textColourId, juce::Colour(ParasiteLookAndFeel::kTextColor).interpolatedWith(juce::Colour(ParasiteLookAndFeel::kBgColor), 0.3f));
    nameEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(ParasiteLookAndFeel::kShadowDark).withAlpha(0.3f));
    nameEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(ParasiteLookAndFeel::kAccentColor));
    nameEditor.setColour(juce::CaretComponent::caretColourId, juce::Colour(ParasiteLookAndFeel::kTextColor));
    nameEditor.onReturnKey = [this] { doSave(); };
    nameEditor.onEscapeKey = [this] { doCancel(); };
    nameEditor.onTextChange = [this] {
        if (awaitingOverwrite)
        {
            awaitingOverwrite = false;
            saveBtn.setButtonText("Save");
            nameEditor.setColour(juce::TextEditor::focusedOutlineColourId,
                juce::Colour(ParasiteLookAndFeel::kAccentColor));
            nameEditor.repaint();
            repaint();
        }
    };
    addAndMakeVisible(nameEditor);

    for (int i = 0; i < kNumCategories; ++i)
    {
        categoryButtons[i].setButtonText(kCategories[i]);
        categoryButtons[i].setMouseClickGrabsKeyboardFocus(false);
        categoryButtons[i].onClick = [this, i] {
            selectedCategory = kCategories[i];
            repaint();
        };
        addAndMakeVisible(categoryButtons[i]);
    }

    saveBtn.setMouseClickGrabsKeyboardFocus(false);
    saveBtn.onClick = [this] { doSave(); };
    addAndMakeVisible(saveBtn);

    cancelBtn.setMouseClickGrabsKeyboardFocus(false);
    cancelBtn.onClick = [this] { doCancel(); };
    addAndMakeVisible(cancelBtn);
}

void SaveOverlay::refresh()
{
    awaitingOverwrite = false;
    saveBtn.setButtonText("Save");

    // Pre-fill with current preset name (user or factory), skip Init/Random
    juce::String prefill;
    if (proc.isUserPreset())
    {
        prefill = proc.getUserPresetName();
    }
    else
    {
        auto& registry = proc.getPresetRegistry();
        int idx = proc.getCurrentPresetIndex();
        if (idx >= 0 && idx < static_cast<int>(registry.size()))
        {
            auto& entry = registry[static_cast<size_t>(idx)];
            if (entry.category != "Init")
                prefill = entry.name;
        }
    }
    nameEditor.setText(prefill, false);

    // Pre-select category from current preset (factory or user)
    selectedCategory = "Bass";
    {
        auto& registry = proc.getPresetRegistry();
        int idx = proc.getCurrentPresetIndex();
        if (idx >= 0 && idx < static_cast<int>(registry.size()))
        {
            auto& entry = registry[static_cast<size_t>(idx)];
            for (auto& cat : kCategories)
                if (entry.category == cat) { selectedCategory = cat; break; }
        }
    }

    // Refresh theme colours
    nameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(ParasiteLookAndFeel::kDisplayBg));
    nameEditor.setColour(juce::TextEditor::textColourId, juce::Colour(ParasiteLookAndFeel::kTextColor).interpolatedWith(juce::Colour(ParasiteLookAndFeel::kBgColor), 0.3f));
    nameEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(ParasiteLookAndFeel::kShadowDark).withAlpha(0.3f));
    nameEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(ParasiteLookAndFeel::kAccentColor));
    nameEditor.setColour(juce::CaretComponent::caretColourId, juce::Colour(ParasiteLookAndFeel::kTextColor));

    nameEditor.selectAll();
    repaint();
}

void SaveOverlay::lookAndFeelChanged()
{
    nameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(ParasiteLookAndFeel::kDisplayBg));
    nameEditor.setColour(juce::TextEditor::textColourId, juce::Colour(ParasiteLookAndFeel::kTextColor).interpolatedWith(juce::Colour(ParasiteLookAndFeel::kBgColor), 0.3f));
    nameEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(ParasiteLookAndFeel::kShadowDark).withAlpha(0.3f));
    nameEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(ParasiteLookAndFeel::kAccentColor));
    nameEditor.setColour(juce::CaretComponent::caretColourId, juce::Colour(ParasiteLookAndFeel::kTextColor));
    nameEditor.applyFontToAllText(nameEditor.getFont(), true);
    repaint();
}

void SaveOverlay::doSave()
{
    auto name = nameEditor.getText().trim();
    if (name.isEmpty())
        return;

    // Check if a preset with same name already exists
    auto& registry = proc.getPresetRegistry();
    bool exists = false;
    for (auto& entry : registry)
    {
        if (entry.name.equalsIgnoreCase(name))
        {
            // Factory presets can't be overwritten — block
            if (entry.isFactory)
            {
                nameEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xFFE57373));
                nameEditor.repaint();
                auto safeThis = juce::Component::SafePointer<SaveOverlay>(this);
                juce::Timer::callAfterDelay(800, [safeThis] {
                    if (safeThis != nullptr)
                    {
                        safeThis->nameEditor.setColour(juce::TextEditor::focusedOutlineColourId,
                            juce::Colour(ParasiteLookAndFeel::kAccentColor));
                        safeThis->nameEditor.repaint();
                    }
                });
                return;
            }
            exists = true;
            break;
        }
    }

    // If a user preset with same name exists, ask for overwrite confirmation
    if (exists && !awaitingOverwrite)
    {
        confirmOverwrite(name);
        return;
    }

    // Save (overwrites existing file if same name)
    awaitingOverwrite = false;
    saveBtn.setButtonText("Save");
    proc.saveUserPreset(name, selectedCategory);
    proc.buildPresetRegistry();

    // Select the newly saved preset
    auto& newRegistry = proc.getPresetRegistry();
    for (int i = 0; i < static_cast<int>(newRegistry.size()); ++i)
    {
        if (newRegistry[static_cast<size_t>(i)].name.equalsIgnoreCase(name))
        {
            proc.loadPresetAt(i);
            break;
        }
    }

    if (onSave) onSave();
}

void SaveOverlay::confirmOverwrite(const juce::String& /*name*/)
{
    awaitingOverwrite = true;
    saveBtn.setButtonText("Overwrite");

    // Highlight outline orange to signal overwrite
    nameEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xFFFF9800));
    nameEditor.repaint();
    repaint();
}

void SaveOverlay::doCancel()
{
    awaitingOverwrite = false;
    saveBtn.setButtonText("Save");
    if (onCancel) onCancel();
}

void SaveOverlay::resized()
{
    auto area = getLocalBounds();

    // Center content vertically
    int contentH = 160;
    auto content = area.withSizeKeepingCentre(juce::jmin(400, area.getWidth() - 40), contentH);

    // Title space (painted in paint())
    content.removeFromTop(30);

    // Name editor
    nameEditor.setBounds(content.removeFromTop(32).reduced(20, 0));
    content.removeFromTop(16);

    // Category buttons row
    auto catRow = content.removeFromTop(28).reduced(20, 0);
    int catW = (catRow.getWidth() - 5 * kNumCategories) / kNumCategories;
    for (int i = 0; i < kNumCategories; ++i)
    {
        categoryButtons[i].setBounds(catRow.removeFromLeft(catW));
        catRow.removeFromLeft(6);
    }

    content.removeFromTop(20);

    // Save / Cancel row
    auto btnRow = content.removeFromTop(30);
    int btnW = 80;
    int gap = 12;
    int totalW = btnW * 2 + gap;
    auto centered = btnRow.withSizeKeepingCentre(totalW, 30);
    cancelBtn.setBounds(centered.removeFromLeft(btnW));
    centered.removeFromLeft(gap);
    saveBtn.setBounds(centered.removeFromLeft(btnW));
}

void SaveOverlay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(ParasiteLookAndFeel::kBgColor));

    // Title
    auto area = getLocalBounds();
    int contentH = 160;
    auto content = area.withSizeKeepingCentre(juce::jmin(400, area.getWidth() - 40), contentH);
    auto titleArea = content.removeFromTop(30);

    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::bold));
    if (awaitingOverwrite)
    {
        g.setColour(juce::Colour(0xFFFF9800));
        g.drawText("Overwrite existing preset?", titleArea, juce::Justification::centred);
    }
    else
    {
        g.setColour(juce::Colour(ParasiteLookAndFeel::kTextColor));
        g.drawText("Save Preset", titleArea, juce::Justification::centred);
    }

    // Style category pills: highlight selected
    for (int i = 0; i < kNumCategories; ++i)
    {
        bool sel = (juce::String(kCategories[i]) == selectedCategory);
        categoryButtons[i].setColour(juce::TextButton::textColourOnId,
            sel ? juce::Colour(ParasiteLookAndFeel::kAccentColor)
                : juce::Colour(ParasiteLookAndFeel::kTextColor));
        categoryButtons[i].setColour(juce::TextButton::textColourOffId,
            sel ? juce::Colour(ParasiteLookAndFeel::kAccentColor)
                : juce::Colour(ParasiteLookAndFeel::kTextColor));
    }
}
