#pragma once
#include "IResultSink.h"
#include "CsvFormat.h"

class CsvFrameSink : public IResultSink {

public:
	CsvFrameSink(const juce::File& file, const CsvFormat& format = CsvFormat());
	void begin(const juce::StringArray& schema) override;
	void onFileBegin(const juce::String& fileName) override;
	void onFrame(const float* values, int numValues) override;
	void end() override;

	bool isNull() const { return outputStream == nullptr; }

private:
	void open(const juce::File& file);
	juce::String format(float value);
	juce::String fileSelected;
	int frame = 0;
	CsvFormat csvFormat;
	std::unique_ptr<FileOutputStream> outputStream;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CsvFrameSink)
};