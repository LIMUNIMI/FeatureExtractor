#pragma once
#include <JuceHeader.h>
#include "Features.h"
#include "Functionals.h"
#include "FeatureMapper.h"
#include "AudioPlayer.h"

#include "MidiOscSink.h"

class MyFeatureExtractor : public ThreadWithProgressWindow {
public:
	MyFeatureExtractor(AudioDeviceManager &audioDeviceManager);
	~MyFeatureExtractor() override { stopThread(2000); };

	void prepareToPlay(int samplesPerBlockExpected, double sampleRate);
	void getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill);
	void releaseResources();

	AudioPlayer& getAudioPlayer() { return audioPlayer; }
	std::function<void()> onStateChanged;

	//csv
	void setCsvPath(const juce::String& newPath) { csvPath = newPath; }
	juce::String getCsvPath() const { return csvPath; }
	void setCsvFileName(const String& newName) {
		csvFileName = File::createLegalFileName(newName);
	}
	String getCsvFileName() const { return csvFileName; }

	//osc
	void setOscIP(String ip) { oscIP = ip; }
	String getOscIP() const { return oscIP; }
	int getOscPort() const { return oscPort; }
	void setOscPort(int port) { oscPort = port; }
	void connectOsc() { liveSink.connectOsc(oscIP, oscPort); }
	void setOscEnabled(bool enabled) { liveSink.setOscEnabled(enabled); }

	//midi
	bool setMidiOutput(int index) { return liveSink.setMidiOutput(index); };
	StringArray getMidiOutput() const { return midiOutputNames; }
	void setMidiEnabled(bool enabled) { liveSink.setMidiEnabled(enabled); }

	void setUpdateRate(double newHz) { liveSink.setUpdateRate(newHz); }

	//live input
	void setLiveMode(bool shouldBeEnabled) { liveBool = shouldBeEnabled; }
	void setMonitorMode(bool shouldBeEnabled) { monitorBool = shouldBeEnabled; }

	bool isLiveModeOn() const { return liveBool; }
	bool isMonitorOn() { return monitorBool; }

	bool canEditSettings() const { return !audioPlayer.isPlaying() && !isThreadRunning() && !liveBool; }

	//feature stuff
	std::function<void()> onPrepareForBatch;
	void prepareLiveFeatures();
	void setActiveFeatures(std::vector<Feature*> features) { activeFeatures = features; buildLiveSink(); };

	void setActiveFunctionals(std::vector<Functional*> functionals) {
		activeFunctionals = functionals;
	}

private:
	//feature stuff
	std::vector<Functional*> activeFunctionals;
	std::vector<Feature*> activeFeatures;

	//csv
	String csvPath;
	String csvFileName = "Feature_analysis";

	//midi
	StringArray midiOutputNames;

	AudioDeviceManager& deviceManager; //gestione midi i/o

	//osc
	String oscIP = "127.0.0.1";
	int oscPort = 9001;

	//live mode
	std::atomic<bool> liveBool = false;
	std::atomic<bool> monitorBool = false;
	AudioPlayer audioPlayer;
	AudioFormatManager formatManager;
	MidiOscSink liveSink{ deviceManager };
	std::vector<float> liveFrame;
	void buildLiveSink();

	//batch mode
	void processFile(std::vector<File> filesToProcess);
	std::vector<File> filesToProcessRun;
	void run() override;

	//msg sample rate
	double sr = 44100.;
	int bufferSize = 512;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MyFeatureExtractor)
};