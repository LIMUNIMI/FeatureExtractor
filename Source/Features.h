#pragma once
#include <JuceHeader.h>
#include "Spectrogram.h"
#include "Costants.h"

//TODO: commentare procedura aggiunta feature
class Feature {
public:
    Feature() {}

    virtual ~Feature() = default;

	virtual juce::StringArray outputNames() const = 0;    // invariante, chiamato in preparazione
	virtual const std::vector<float>& values() const = 0;  // memoria posseduta dalla feature
    virtual juce::String getName() const = 0;
    virtual void prepareToPlay(double sr, int samplesPerBlock) = 0;
    virtual void releaseResources() = 0;
    virtual void processBlock(juce::AudioBuffer<float>& buffer) = 0;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Feature)
};

class SpectralFeature : public Feature {
public:
	SpectralFeature(int fftOrder) : stft(fftOrder) {}

	void prepareToPlay(double sr, int samplesPerBlock) override;

	void processBlock(AudioBuffer<float>& buffer) override;

	void releaseResources() override { aux.setSize(0, 0); }

	virtual void calculateSpectralFeatures(const std::vector<float>& magnitudes, int fftSize, int numBins) = 0;

protected:
	STFT stft;
	AudioBuffer<float> aux;
	double sampleRate = 44100.;

private:
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralFeature)
};