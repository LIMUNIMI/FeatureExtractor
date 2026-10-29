#pragma once
#include "IResultSink.h"
#include "Features.h"
#include "FeatureMapper.h"

class MidiOscSink : public IResultSink, private Thread {

public:
	MidiOscSink(AudioDeviceManager& audioDeviceManager);
	~MidiOscSink() override { stopThread(2000); };

	void prepareToPlay(int samplesPerBlockExpected, double sampleRate);
	void setFeatures(const std::vector<Feature*>& features);
	void push(const float* values, int numValues, int numSamples);


	void begin(const juce::StringArray& schema) override {};
	void onFrame(const float* values, int numValues) override { push(values, numValues, bufferSize); };

	//osc
	bool connectOsc(String& ip, int port) {
		const ScopedLock sl(lock);
		return oscSender.connect(ip, port); 
	}

	void setOscEnabled(bool enabled) { oscEnabled = enabled; }

	//midi
	bool setMidiOutput(int index);
	void setMidiEnabled(bool enabled) { midiEnabled = enabled; }

	void setUpdateRate(double newHz) { updateRate = newHz; }
	int getNumCol() const { return numCol; }

	static constexpr int N_MAX_COL = 128;

private:

	CriticalSection lock;

	struct Frame {
		int n;
		float val[N_MAX_COL];
	};

	struct Feat { 
		juce::String name; 
		int offset; 
		std::vector<float> values; 
	};

	void run() override;
	bool pop(Frame& frame);
	void send(Frame& frame);


	juce::AbstractFifo queue{64};
	std::array<Frame, 64> frames{};

	std::vector<Feat> featuresLayout;

	
	//midi
	std::atomic<bool> midiEnabled = false;
	MidiBuffer midiBuffer;
	MidiMapper midiMapper;

	AudioDeviceManager& deviceManager; //gestione midi i/o

	//osc
	std::atomic<bool> oscEnabled = false;
	OscMapper oscMapper;
	OSCSender oscSender;

	int numCol = 0;

	//msg sample rate
	double sr = 44100.;
	int bufferSize = 512;
	double updateRate = 50.0;
	int sampleCount = 0;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiOscSink)
};