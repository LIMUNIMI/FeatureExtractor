#pragma once

#include <JuceHeader.h>
#include "Functionals.h"

class FuncList : public juce::Component, public juce::ListBoxModel {
public:
    FuncList() {
        functionals.add(new Average());
        functionals.add(new Median());
        functionals.add(new StdDev());
        functionals.add(new IQR());
        functionals.add(new Max());
        functionals.add(new Min());

        addAndMakeVisible(&functionalsGroup);
        functionalsGroup.setText("Functionals");
        functionalsGroup.setTextLabelPosition(juce::Justification::centredLeft);
        functionalsGroup.setColour(juce::GroupComponent::textColourId, juce::Colours::white);
        functionalsGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colours::white.withAlpha(0.4f));

        addAndMakeVisible(&funcListBox);
        funcListBox.setModel(this);

        funcListBox.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        funcListBox.setRowHeight(25);
        funcListBox.setMultipleSelectionEnabled(true);
        funcListBox.setClickingTogglesRowSelection(true);
    }

    ~FuncList() override {}

    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colours::transparentBlack);
    }

    void resized() override {
        auto area = getLocalBounds();
        functionalsGroup.setBounds(area);
        funcListBox.setBounds(area.reduced(5, 5).withTrimmedTop(15));
    }

    int getNumRows() override {
        return (int)functionals.size();
    }

    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override {
        if (rowIsSelected) {

            g.fillAll(juce::Colours::white.withAlpha(0.1f));

            g.setColour(juce::Colours::white);
            g.setFont(juce::Font("Sans-Serif", 18.0f, juce::Font::plain));
            g.drawText(juce::CharPointer_UTF8("\xe2\x9c\x93"), -10, 0, width, height, juce::Justification::centredRight, false);
        }

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font("Sans-Serif", 14.0f, juce::Font::plain));

        if (rowNumber < (int)functionals.size()) {
            g.drawText(functionals[rowNumber]->getName(), 10, 0, width, height, juce::Justification::centredLeft);
        }
    }

    void selectedRowsChanged(int lastRowSelected) override {}

    bool isRowSelected(int row) {
        return funcListBox.isRowSelected(row);
    }

    juce::OwnedArray<Functional>& getFunctionals() { return functionals; }

private:
    juce::ListBox funcListBox;
    juce::OwnedArray<Functional> functionals;
    juce::GroupComponent functionalsGroup; 

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FuncList)
};