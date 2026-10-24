#pragma once
#include <JuceHeader.h>
#include "Features.h"

class SpectralMoments : public SpectralFeature {
public:
	SpectralMoments() : SpectralFeature(10) {} ;
	~SpectralMoments() {};

	void prepareToPlay(double sr, int samplesPerBlock) override {
		SpectralFeature::prepareToPlay(sr, samplesPerBlock);
		centroid = 0.0f;
		spread = 0.0f;
		skewness = 0.0f;
		kurtosis = 0.0f;
		moments = { 0.0f, 0.0f, 0.0f, 0.0f };
	}

	String getName() const override { return "Spectral Moments"; }

	juce::StringArray outputNames() const override { return { "Centroid", "Spread", "Skewness", "Kurtosis" }; };
	const std::vector<float>& values() const override { return moments; };

	void calculateSpectralFeatures(const std::vector<float>& magnitudes, int fftSize, int numBins) override;

private:
	float centroid = 0.0f;
	float spread = 0.0f;
	float skewness = 0.0f;
	float kurtosis = 0.0f;
	std::vector<float> moments = { 0.0f, 0.0f, 0.0f, 0.0f };

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralMoments)
};

class F0 : public SpectralFeature {
public:
	F0() : SpectralFeature(12) {};
	~F0() {};

	void prepareToPlay(double sr, int samplesPerBlock) override {
		SpectralFeature::prepareToPlay(sr, samplesPerBlock);
		mSampleRate = sampleRate;
		mBlockSize = samplesPerBlock; 
		setupAlgorithmParameters(); 
		mCurrentPitchHz = 0.0f;
		mCurrentStrength = 0.0f;
		f0 = { 0.0f };
	}

	String getName() const override { return "Fundamental frequency"; }

	juce::StringArray outputNames() const override { return "Fundamental Frequency"; };
	const std::vector<float>& values() const override { return f0; };

	void calculateSpectralFeatures(const std::vector<float>& magnitudes, int fftSize, int numBins) override;

private:

	// Parametri base 
	double mSampleRate = 44100.0;
	int mBlockSize = 512;
	float mCurrentPitchHz = 0.0f;
	float mCurrentStrength = 0.0f;
	std::vector<float> f0 = { 0.0f };

	// Parametri algoritmo SWIPE 
	double mMinFreq = 80.0;
	double mMaxFreq = 2000.0;
	double mStrengthThreshold = 0.15; // DEBUG !!!! 0.5 

	// Strutture dati SWIPE
	std::vector<double> mPitchCandidates;
	std::vector<double> mErbFrequencies;
	std::vector<int> mPrimes;

	// Matrici per analisi 
	std::vector<std::vector<float>> mLoudnessMatrix;
	std::vector<float> mStrengthVector;

	//setup
	void setupAlgorithmParameters();
	void generatePitchCandidates();
	void generatePrimes(int maxHarmonic);

	//analisi
	void calculateStrength();
	void findPitch();

	// Funzioni matematiche 
	double hzToErb(double hz);
	double erbToHz(double erb);
	std::vector<int> sieve(int n);

	float interpolatePeak(const std::vector<float>& data, int peakIndex);

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(F0)
};

class Chromagram : public SpectralFeature {
public:
	Chromagram() : SpectralFeature(12) {};
	~Chromagram() {};

	void prepareToPlay(double sr, int samplesPerBlock) override;

	String getName() const override { return "Chromagram"; }

	void releaseResources() override {
		SpectralFeature::releaseResources();
		std::fill(chroma.begin(), chroma.end(), 0.0f);
	}

	juce::StringArray outputNames() const override { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; };
	const std::vector<float>& values() const override { return chroma; };
	
	void calculateSpectralFeatures(const std::vector<float>& magnitudes, int fftSize, int numBins) override;

private:
	std::vector<float> chroma = std::vector<float>(12, 0.0f);

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Chromagram)
};