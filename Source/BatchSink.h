#pragma once
#include "IResultSink.h"

class BatchSink : public IResultSink {

public:
	BatchSink() {};

	void begin(const juce::StringArray& schema) override {
		for (auto* sink : sinks) {
			sink->begin(schema);
		}
	};

	void onFileBegin(const juce::String& fileName) override {
		for (auto* sink : sinks) {
			sink->onFileBegin(fileName);
		}
	};

	void onFrame(const float* values, int numValues) override {
		for (auto* sink : sinks) {
			sink->onFrame(values, numValues);
		}
	};

	void onFileEnd(const juce::String& fileName) override {
		for (auto* sink : sinks) {
			sink->onFileEnd(fileName);
		}
	};

	void end() override {
		for (auto* sink : sinks) {
			sink->end();
		}
	};

	void add(IResultSink* sink) { 
		if (sink != nullptr) {
			sinks.push_back(sink);
		}
	};

private:
	std::vector<IResultSink*> sinks;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BatchSink)
};