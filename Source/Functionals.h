#pragma once
#include <JuceHeader.h>
#include "FeatureResult.h"

class Functional {
public:
	Functional() {}
	virtual ~Functional() = default;

	virtual void store(const float* values, int numValues) = 0;
	virtual String getName() const = 0;
	virtual void reset(int numValues) { n = numValues; onReset(); };
	virtual std::vector<float> getResult()  = 0;

protected:
	virtual void onReset() = 0;
	int n = 0;
private:
	
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Functional)
};

class Average : public Functional {
public:
	Average() {};
	~Average() {};

	void store(const float* values, int numValues) override;
	String getName() const override { return "Average"; }
	std::vector<float> getResult() override;

protected:
	void onReset() override { count = 0; sums.assign(n, 0.0);};
private:
	std::vector<double> sums;
	int count = 0;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Average)
};

class Median : public Functional {
public:
	Median() {};
	~Median() {};

	void store(const float* values, int numValues) override;
	String getName() const override { return "Median"; }
	std::vector<float> getResult() override;

protected:
	void onReset() override { samples.clear(); samples.resize(n); };

private:
	std::vector <std::vector<float>> samples;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Median)
};

class StdDev : public Functional {
public:
	StdDev() {};
	~StdDev() {};

	void store(const float* values, int numValues) override;
	String getName() const override { return "Standard deviation"; }
	std::vector<float> getResult() override;

protected:
	void onReset() override { sums.assign(n, 0.0); sumSquares.assign(n, 0.0); count = 0; };

private:
	int count = 0;
	std::vector<double> sums, sumSquares;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StdDev)
};

class IQR : public Functional {
public:
	IQR() {};
	~IQR() {};

	void store(const float* values, int numValues) override;
	String getName() const override { return "IQR"; }
	std::vector<float> getResult() override;

protected:
	void onReset() override { samples.clear(); samples.resize(n); };

private:
	std::vector <std::vector<float>> samples;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IQR)
};

class Max : public Functional {
public:
	Max() {};
	~Max() {};

	void store(const float* values, int numValues) override;
	String getName() const override { return "Max"; }
	std::vector<float> getResult() override;

protected:
	void onReset() override { maxes.assign(n, std::numeric_limits<float>::lowest()); };

private:
	std::vector<float> maxes;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Max)
};

class Min : public Functional {
public:
	Min() {};
	~Min() {};

	void store(const float* values, int numValues) override;
	String getName() const override { return "Min"; }
	std::vector<float> getResult() override;

protected:
	void onReset() override { mins.assign(n, std::numeric_limits<float>::max()); };

private:
	std::vector<float> mins;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Min)
};



