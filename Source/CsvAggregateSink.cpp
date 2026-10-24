#include "CsvAggregateSink.h"

CsvAggregateSink::CsvAggregateSink(const juce::File& file, std::vector<Functional*> functionalsSelected)
	: activeFunctionals(std::move(functionalsSelected))
{
    open(file);
}

void CsvAggregateSink::open(const juce::File& file) {

    if (file.existsAsFile()) {
        file.deleteFile();
    }

    auto stream = std::make_unique<FileOutputStream>(file);

    if (stream->failedToOpen()) {
        return;
    }

    outputStream = std::move(stream);
}

void CsvAggregateSink::begin(const juce::StringArray& schema) {

    if (!outputStream) {
        return;
    };

    numCol = schema.size();

    String header = "File_Name";
    for (auto* func : activeFunctionals) {
            for (const auto& name : schema) {
                header << csvFormat.sep << name << "_" << func->getName();
            }
        }

    outputStream->writeText(header + "\n", false, false, nullptr);
}

void CsvAggregateSink::onFileBegin(const juce::String& fileName) {
    
    for (auto* func : activeFunctionals) {
        func->reset(numCol);
    }

}

void CsvAggregateSink::onFrame(const float* values, int numValues) {
    
    for (auto* func : activeFunctionals) {
        func->store(values, numValues);
    }
}

void CsvAggregateSink::onFileEnd(const juce::String& fileName) {

    if (!outputStream) { 
        return; 
    };
    
    String riga = fileName;

    for (auto* func : activeFunctionals) {
        auto res = func->getResult();
        for (int n = 0; n < numCol; n++) {
            riga << csvFormat.sep;
            if (n < res.size()) {
                riga << format(res[n]);
            }
        }
    }
    outputStream->writeText(riga + "\n", false, false, nullptr);
}

juce::String CsvAggregateSink::format(float value) {

    return String(value, csvFormat.precision).replace(".", csvFormat.decimalSep);
}

void CsvAggregateSink::end() {

    if (outputStream) {
        outputStream->flush();
    }
}