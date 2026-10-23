#pragma once
#include "Features.h"
#include <JuceHeader.h>

class RRMS : public Feature {
public:
	RRMS(float defaultTime = 0.035f, float defaultMaxTime = 1.0f) : rmsTime(defaultTime), maxTime(defaultMaxTime) {}

	~RRMS(){}

	String getName() const override { return "RRMS"; }

	void prepareToPlay(double, int) override;

	void releaseResources() override;

	void processBlock(AudioBuffer<float>& buffer) override;

	void createResultPackage(FeatureResult& featPackage) override {
		featPackage.add("RMS", rmsValue);
	}

	void getResult(FeatureResult& featPackage) override {
		createResultPackage(featPackage);
	};

private:
	void reset();

	AudioBuffer<float> aux;
	float rmsValue = 0.0f;
	float avg = 0;
	AudioBuffer<float> history;
	int historyIndex = 0;
	float maxTime;
	double sampleRate = 44100.;
	float rmsTime;
	int windowSize = 1543;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RRMS)
};

class PAN : public Feature {
public:
	PAN() {}

	~PAN() {}

	String getName() const override { return "PAN"; }

	void prepareToPlay(double, int) override {};

	void releaseResources() override {};

	void processBlock(AudioBuffer<float>& buffer) override;

	void createResultPackage(FeatureResult& featPackage) override {
		featPackage.add("PAN", panValue);
	}

	void getResult(FeatureResult& featPackage) override {
		createResultPackage(featPackage);
	};

private:
	float panValue = 0.0f;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PAN)
};

class Brightness : public Feature {
public:
	Brightness() {}

	~Brightness() {}

	String getName() const override { return "Brightness"; }

	void prepareToPlay(double sr, int samplesPerBlock) override;

	void releaseResources() override;

	void processBlock(AudioBuffer<float>& buffer) override;

	void createResultPackage(FeatureResult& featPackage) override {
		featPackage.add("Brightness", brightValue);
	}

	void getResult(FeatureResult& featPackage) override {
		createResultPackage(featPackage);
	}

private:
	AudioBuffer<float> filterBuffer;
	float brightValue = 0.0f;
	double sampleRate = 44100.;
	RRMS envelope, filterEnvelope;
	float lastSample = 0.0f;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Brightness)
};

class ZCR : public Feature {
public:
	ZCR() {}

	~ZCR() {}

	String getName() const override { return "ZCR"; }

	void prepareToPlay(double, int) override { lastSign = 0; zcrValue = 0.0f; };

	void releaseResources() override {};

	void processBlock(AudioBuffer<float>& buffer) override;

	void createResultPackage(FeatureResult& featPackage) override {
		featPackage.add("ZCR", zcrValue);
	}

	void getResult(FeatureResult& featPackage) override {
		createResultPackage(featPackage);
	}

private:
	float zcrValue = 0.0f;
	int lastSign = 0;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZCR)
};