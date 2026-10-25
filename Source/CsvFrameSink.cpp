#include "CsvFrameSink.h"

CsvFrameSink::CsvFrameSink(const juce::File& file, const CsvFormat& format) : csvFormat(format)
{
    open(file);
}

void CsvFrameSink::open(const juce::File& file) {

    if (file.existsAsFile()) {
        file.deleteFile();
    }

    auto stream = std::make_unique<FileOutputStream>(file);

    if (stream->failedToOpen()) {
        return;
    }

    outputStream = std::move(stream);
}

void CsvFrameSink::begin(const juce::StringArray& schema) {

    if (!outputStream) {
        return;
    };

    String header = "File_Name";
    header << csvFormat.sep << "Frame";
    
    for (const auto& name : schema) {
        
        header << csvFormat.sep << name;
    }

    outputStream->writeText(header + "\n", false, false, nullptr);
}

void CsvFrameSink::onFileBegin(const juce::String& fileName) {

    frame = 0;
    fileSelected = fileName;
}

void CsvFrameSink::onFrame(const float* values, int numValues) {

    if (!outputStream) {
        return;
    };

    String riga = fileSelected;
    riga << csvFormat.sep << String(frame++);

    for (int n = 0; n < numValues; n++) {
        riga << csvFormat.sep << format(values[n]);
    }

    outputStream->writeText(riga + "\n", false, false, nullptr);
}

juce::String CsvFrameSink::format(float value) {

    return String(value, csvFormat.precision).replace(".", csvFormat.decimalSep);
}

void CsvFrameSink::end() {

    if (outputStream) {
        outputStream->flush();
    }
}