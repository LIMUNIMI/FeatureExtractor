#pragma once
#include <JuceHeader.h>

struct CsvFormat {
	juce::String sep = ";";
	juce::String decimalSep = ",";
	int precision = 4;
};

enum class Export {
	Aggregate,
	Frame,
	Both
};