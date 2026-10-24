#pragma once
#include <JuceHeader.h>

class IResultSink {
public:
    virtual ~IResultSink() = default;
    virtual void begin(const juce::StringArray& schema) = 0;
    virtual void onFileBegin(const juce::String& fileName) {}
    virtual void onFrame(const float* values, int numValues) = 0;
    virtual void onFileEnd(const juce::String& fileName) {}
    virtual void end() {}
};
