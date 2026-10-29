#include "MidiOscSink.h"

MidiOscSink::MidiOscSink(AudioDeviceManager& audioDeviceManager): Thread("MidiOscSink"), deviceManager(audioDeviceManager)
{
	startThread();
}

bool MidiOscSink::setMidiOutput(int index)
{
    auto list = MidiOutput::getAvailableDevices();
    if (list.size() > 0)
    {
        auto newInput = list[index];
        const ScopedLock sl(lock);
        deviceManager.setDefaultMidiOutputDevice(newInput.identifier);
        return true;
    }
    return false;
}

void MidiOscSink::setFeatures(const std::vector<Feature*>& features)
{
    std::vector<Feat> layout;
    int offset = 0;

    for (auto& f : features) {
        int count = f->outputNames().size();
        Feat feat = { f->getName(), offset, std::vector<float>(count, 0.0f) };
        layout.push_back(feat);
        offset += count;
    }
    
    const ScopedLock sl(lock);
    featuresLayout = std::move(layout);
    numCol = offset;
}

void MidiOscSink::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    sr = sampleRate;
    bufferSize = samplesPerBlockExpected;
    sampleCount = static_cast<int>(sr / updateRate);
}

void MidiOscSink::push(const float* values, int numValues, int numSamples)
{
    sampleCount -= numSamples;

    if (sampleCount > 0) {
        return;
    }

    sampleCount = static_cast<int>(sr / updateRate);

    if (!midiEnabled && !oscEnabled) {
        return;
    }

    if (numValues <= 0 || numValues > N_MAX_COL) return;

    int start1, size1, start2, size2;
    queue.prepareToWrite(1, start1, size1, start2, size2);

    if (size1 > 0) {
        Frame& frame = frames[start1];
        frame.n = numValues;
        std::copy(values, values + numValues, frame.val);
        queue.finishedWrite(1);
    }
}

void MidiOscSink::run()
{
    Frame frame;

    while (!threadShouldExit()) {
        bool error = true;

        while (pop(frame)) {
            send(frame);
            error = false;
        }

        if (error) {
            wait(5);
        }
    }
}

bool MidiOscSink::pop(Frame& frame)
{
    int start1, size1, start2, size2;
    queue.prepareToRead(1, start1, size1, start2, size2);

    if (size1 > 0) {
        frame = frames[start1];
        queue.finishedRead(1);

        return true;
    }

    return false;
}


void MidiOscSink::send(Frame& frame)
{
    const ScopedLock sl(lock);

    if (frame.n != numCol) {
        return;
    }

    midiBuffer.clear();
    for (auto& feat : featuresLayout) {

        std::copy(frame.val + feat.offset, frame.val + feat.offset + feat.values.size(), feat.values.begin());

        if (midiEnabled) {
            midiMapper.toMidi(feat.values, feat.name, midiBuffer);
        }
        if (oscEnabled) {
            oscMapper.toOsc(feat.values, feat.name, oscSender);
        }
    }

    if (midiEnabled && !midiBuffer.isEmpty()) {
        if (auto* output = deviceManager.getDefaultMidiOutput()) {
            output->sendBlockOfMessagesNow(midiBuffer);
        }
    }
}