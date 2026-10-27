#include "ExtractionEngine.h"
#include "TemporalFeatures.h"
#include "SpectralFeatures.h"
#include "CsvAggregateSink.h"
#include "CsvFrameSink.h"
#include "BatchSink.h"


#define BATCH_BLOCK_SIZE 1024

MyFeatureExtractor::MyFeatureExtractor(AudioDeviceManager& audioDeviceManager) : 
    audioPlayer(formatManager), ThreadWithProgressWindow("Processing files...", true, true), deviceManager(audioDeviceManager) 
{
	csvPath = File::getSpecialLocation(File::userHomeDirectory).getFullPathName();

    audioPlayer.onProcessRequested = [this](std::vector<File> filesToProcess) {
        processFile(filesToProcess);
    };

    audioPlayer.onPlaybackStopped = [this] {

        if (onStateChanged) onStateChanged();
    };

    auto midiOutputs = MidiOutput::getAvailableDevices();
    for (auto output : midiOutputs) midiOutputNames.add(output.name);

    formatManager.registerBasicFormats();

    if (!liveSink.connectOsc(oscIP, oscPort)) {
        AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Connection error", "Error: could not send OSC message.", "OK");
    }

    deviceManager.initialise(2, 2, nullptr, true);
}

void MyFeatureExtractor::buildLiveSink() {

    liveSink.prepareToPlay(bufferSize, sr);
    liveSink.setFeatures(activeFeatures);
    liveFrame.assign(liveSink.getNumCol(), 0.0f);
}

void MyFeatureExtractor::prepareLiveFeatures() {

    for (auto* f : activeFeatures) {
        f->prepareToPlay(sr, bufferSize);
    }
    buildLiveSink();
}

void MyFeatureExtractor::prepareToPlay(int samplesPerBlockExpected, double sampleRate) 
{
    sr = sampleRate;
    bufferSize = samplesPerBlockExpected;
    prepareLiveFeatures();
    audioPlayer.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void MyFeatureExtractor::getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill) {
    if (!liveBool) {
        bufferToFill.clearActiveBufferRegion();
        audioPlayer.getNextAudioBlock(bufferToFill);
    }

    if (bufferToFill.buffer->getNumChannels() > 0 && bufferToFill.numSamples > 0) {
        if (audioPlayer.isPlaying() || liveBool) {
            juce::AudioBuffer<float> proxyBuffer(bufferToFill.buffer->getArrayOfWritePointers(),
                bufferToFill.buffer->getNumChannels(),
                bufferToFill.startSample,
                bufferToFill.numSamples);

            int numValues = 0;
            for (auto f : activeFeatures) {
                f->processBlock(proxyBuffer);
                for (float val : f->values()) {
                    if (numValues < liveFrame.size()) {
                        liveFrame[numValues++] = val;
                    }
                }
            }

            liveSink.push(liveFrame.data(), numValues, bufferToFill.numSamples);
        }
    }
}

void MyFeatureExtractor::processFile(std::vector<File> filesToProcess) {

    if (onPrepareForBatch) onPrepareForBatch();

    if (filesToProcess.empty()) return;

    filesToProcessRun = filesToProcess;

    ThreadWithProgressWindow::launchThread();
}

void MyFeatureExtractor::run() {
    auto startTime = Time::getMillisecondCounterHiRes();
    bool processCancelled = false;

    CsvAggregateSink aggSink(File(csvPath).getChildFile(csvFileName + ".csv"), activeFunctionals);
    CsvFrameSink frameSink(File(csvPath).getChildFile(csvFileName + "_frames.csv"));

    bool outputFail = (aggSink.isNull() || frameSink.isNull());

    if (!outputFail) {

        BatchSink sink;
        sink.add(&aggSink);
        sink.add(&frameSink);

        StringArray schema;
        for (auto* f : activeFeatures) {
            schema.addArray(f->outputNames());
        }
        int numCol = schema.size();

        sink.begin(schema);
        std::vector<float> frame(numCol);

        int progressCount = 0;
        for (const auto& file : filesToProcessRun) {

            std::unique_ptr<AudioFormatReader> reader(formatManager.createReaderFor(file));
            if (reader != nullptr) {
                for (auto* f : activeFeatures) {
                    f->prepareToPlay(reader->sampleRate, BATCH_BLOCK_SIZE);
                }
                
                sink.onFileBegin(file.getFileName());

                AudioBuffer<float> buffer(reader->numChannels, BATCH_BLOCK_SIZE);
                int64 startSample = 0;

                while (startSample < reader->lengthInSamples) {
                    if (threadShouldExit()) { processCancelled = true; break; }

                    reader->read(&buffer, 0, BATCH_BLOCK_SIZE, startSample, true, true);

                    int i = 0;
                    for (auto* f : activeFeatures) {
                        f->processBlock(buffer);
                        for (float val : f->values()) {
                            frame[i++] = val;
                        }
                    }
                    sink.onFrame(frame.data(), numCol);

                    startSample += BATCH_BLOCK_SIZE;
                }

                sink.onFileEnd(file.getFileName());
            }
            setProgress(progressCount / (double)filesToProcessRun.size());
            progressCount++;
        }

        sink.end();
    }

    auto stopTime = Time::getMillisecondCounterHiRes();
    auto totalTimeSeconds = (stopTime - startTime) / 1000.0f;

    if (outputFail) {
        MessageManager::callAsync([this] {
            if (onStateChanged) onStateChanged();
            String message;
            message << "Could not create the CSV file";
            NativeMessageBox::showMessageBoxAsync(AlertWindow::WarningIcon, "Error", message);
            });
    }
    else if (processCancelled) {
        MessageManager::callAsync([this] {
            if (onStateChanged) onStateChanged();
            String message;
            message << "Processing cancelled";
            NativeMessageBox::showMessageBoxAsync(AlertWindow::InfoIcon, "Cancelled", message);
            });
    }
    else {
        MessageManager::callAsync([this, totalTimeSeconds, count = filesToProcessRun.size()] {
            if (onStateChanged) onStateChanged();
            String message;
            message << "Batch completed! " << count << " files processed in " << String(totalTimeSeconds, 2) << "s.";
            NativeMessageBox::showMessageBoxAsync(AlertWindow::InfoIcon, "Success", message);
            });
    }
}

void MyFeatureExtractor::releaseResources() {
    audioPlayer.releaseResources();
    for (auto* f : activeFeatures) f->releaseResources();
}