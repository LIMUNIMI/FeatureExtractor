#pragma once
#include <algorithm>
#include <JuceHeader.h>
#include "Features.h"

class MidiMapper {
public:
	MidiMapper() {};
	~MidiMapper() {};

	void toMidi(const std::vector<float>& res, String name, MidiBuffer& midiMessages);
	void resetValues() { lastNote = -1; lastChord.clear(); }

private:
	int lastNote = -1;
	std::vector<int> lastChord;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiMapper)
};

class OscMapper {
public: 
	OscMapper() {};
	~OscMapper() {};

	void toOsc(const std::vector<float>& res, String name, OSCSender& sender);

private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscMapper)
};