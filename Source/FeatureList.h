#pragma once

#include <JuceHeader.h>
#include "Features.h"
#include "TemporalFeatures.h"
#include "SpectralFeatures.h"

class FeatList : public juce::Component, public juce::ListBoxModel {
public:
    FeatList() {
        features.add(new RRMS());
        features.add(new PAN());
        features.add(new Brightness());
        features.add(new ZCR());
        features.add(new SpectralMoments());
        features.add(new Chromagram());
        features.add(new F0());

        addAndMakeVisible(&featuresGroup);
        featuresGroup.setText("Features");
        featuresGroup.setTextLabelPosition(juce::Justification::centredLeft);
        featuresGroup.setColour(juce::GroupComponent::textColourId, juce::Colours::white);
        featuresGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colours::white.withAlpha(0.4f));

        addAndMakeVisible(&featListBox);
        featListBox.setModel(this);
        featListBox.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        featListBox.setRowHeight(25);
        featListBox.setMultipleSelectionEnabled(true);
        featListBox.setClickingTogglesRowSelection(true);
    }

    ~FeatList() override {}

    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colours::transparentBlack);
    };

    void resized() override {
        auto area = getLocalBounds();

        featuresGroup.setBounds(area);

        featListBox.setBounds(area.reduced(5, 5).withTrimmedTop(15));
    }

    int getNumRows() override {
        return (int)features.size();
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

        if (rowNumber < (int)features.size()) {
            g.drawText(features[rowNumber]->getName(), 10, 0, width, height, juce::Justification::centredLeft);
        }
    }

    void selectedRowsChanged(int lastRowSelected) override {}

    bool isRowSelected(int row) {
        return featListBox.isRowSelected(row);
    }

    juce::OwnedArray<Feature>& getFeatures() { return features; }

private:
    juce::ListBox featListBox;
    juce::OwnedArray<Feature> features;
    juce::GroupComponent featuresGroup;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FeatList)
};