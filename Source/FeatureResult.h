#pragma once
#include <JuceHeader.h>
constexpr float SILENCE_THRESH_HARD = 1e-6f;
constexpr float SILENCE_THRESH_SOFT = 1e-3f;


//TODO: importanza ordine lettura featureResult
struct FeatureResult {
    juce::StringArray names;
    std::vector<float> values;

    void add(juce::String name, float value) {
        names.add(name);
        values.push_back(value);
    }

    bool isEmpty() const { return values.empty(); }
};
