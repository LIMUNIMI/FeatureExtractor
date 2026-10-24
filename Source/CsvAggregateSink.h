#pragma once
#include "IResultSink.h"
#include "Functionals.h"

struct CsvFormat {
	juce::String sep = ";";
	juce::String decimalSep = ",";
	int precision = 4;
};

class CsvAggregateSink : public IResultSink {

public:
	CsvAggregateSink(const juce::File& file, std::vector<Functional*> functionalsSelected);
	void begin(const juce::StringArray& schema) override;
	void onFileBegin(const juce::String& fileName) override;
	void onFrame(const float* values, int numValues) override;
	void onFileEnd(const juce::String& fileName) override;
	void end() override;

	bool isNull() const { return outputStream == nullptr; }

private:
	void open(const juce::File& file);
	juce::String format(float value);
	int numCol = 0;
	CsvFormat csvFormat;
	std::unique_ptr<FileOutputStream> outputStream;
	std::vector<Functional*> activeFunctionals;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CsvAggregateSink)
};