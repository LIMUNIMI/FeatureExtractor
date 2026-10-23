#pragma once
#include <JuceHeader.h>
#include "FeatureResult.h"

class Functional {
public:
	Functional() {}
	virtual ~Functional() = default;

	virtual void store(const FeatureResult& res) = 0;
	virtual String getName() const = 0;
	virtual void reset() = 0;
	virtual FeatureResult getResult()  = 0;

protected:
	StringArray savedNames;
private:
	
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Functional)
};

class Average : public Functional {
public:
	Average() {};
	~Average() {};

	void store(const FeatureResult& res) override;
	String getName() const override { return "Average"; }
	void reset() override { sums.clear(); count = 0; savedNames.clear(); }
	FeatureResult getResult() override;
	//Functional* clone() const override { return new Average(); }

private:
	std::vector<double> sums;
	int count = 0;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Average)
};

class Median : public Functional {
public:
	Median() {};
	~Median() {};

	void store(const FeatureResult& res) override;
	String getName() const override { return "Median"; }
	void reset() override { values.clear(); savedNames.clear(); }
	FeatureResult getResult() override;
	//Functional* clone() const override { return new Median(); }

private:
	std::vector <std::vector<float>> values;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Median)
};

class StdDev : public Functional {
public:
	StdDev() {};
	~StdDev() {};

	void store(const FeatureResult& res) override;
	String getName() const override { return "Standard deviation"; }
	void reset() override { sums.clear(); sumSquares.clear(); count = 0; savedNames.clear(); }
	FeatureResult getResult() override;
	//Functional* clone() const override { return new StdDev(); }

private:
	int count = 0;
	std::vector<double> sums, sumSquares;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StdDev)
};

class IQR : public Functional {
public:
	IQR() {};
	~IQR() {};

	void store(const FeatureResult& res) override;
	String getName() const override { return "IQR"; }
	void reset() override { values.clear(); savedNames.clear(); }
	FeatureResult getResult() override;
	//Functional* clone() const override { return new IQR(); }

private:
	std::vector <std::vector<float>> values;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IQR)
};

class Max : public Functional {
public:
	Max() {};
	~Max() {};

	void store(const FeatureResult& res) override;
	String getName() const override { return "Max"; }
	void reset() override { maxes.clear(); savedNames.clear(); }
	FeatureResult getResult() override;

private:
	std::vector<float> maxes;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Max)
};

class Min : public Functional {
public:
	Min() {};
	~Min() {};

	void store(const FeatureResult& res) override;
	String getName() const override { return "Min"; }
	void reset() override { mins.clear(); savedNames.clear(); }
	FeatureResult getResult() override;

private:
	std::vector<float> mins;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Min)
};



