#pragma once
#include <JuceHeader.h>

struct CsvFormat {
	juce::String sep = ";";
	juce::String decimalSep = ",";
	int precision = 4;
};