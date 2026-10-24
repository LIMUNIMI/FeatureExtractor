#include "FeatureMapper.h"
#define MIDI_CHAN 1
#define CC_PAN 10
#define CC_BRIGHTNESS 74
#define CC_RMS 7
#define CC_MOMENTS 16
#define MAX_SKEW_THRESHOLD 10.0f
#define MAX_KURT_THRESHOLD 150.0f
#define CHORD_NOTE_AMT 3
#define DEFAULT_OCTAVE 5

void MidiMapper::toMidi(const std::vector<float>& res, String name, MidiBuffer& midiMessages)
{
    //TODO: nome delle feature enum condiviso (magari file macro)
    //TODO: qua faccio o switch case o mappa feature-cc
    if (res.empty()) return;

    if (name == "PAN") {
        auto panCC = roundToInt(jlimit(0.0f, 127.0f, jmap(res[0], -1.0f, 1.0f, 0.0f, 127.0f)));
        midiMessages.addEvent(MidiMessage::controllerEvent(MIDI_CHAN, CC_PAN, panCC), 0);
    }
    else if (name == "Brightness") {
        auto brightCC = roundToInt(jlimit(0.0f, 127.0f, jmap(log10(jlimit(20.0f, 20000.0f, res[0])), log10(20.0f), log10(20000.0f), 0.0f, 127.0f)));
        midiMessages.addEvent(MidiMessage::controllerEvent(MIDI_CHAN, CC_BRIGHTNESS, brightCC), 0);
    }
    else if (name == "RRMS") {
        float dbRms = Decibels::gainToDecibels(res[0], -48.0f);
        auto rmsValue = jlimit(1, 127, roundToInt(jmap(dbRms, -48.0f, 0.0f, 1.0f, 127.0f)));
        midiMessages.addEvent(MidiMessage::controllerEvent(MIDI_CHAN, CC_RMS, rmsValue), 0);
    }
    else if (name == "Spectral Moments") {
        auto centroidValue = roundToInt(jlimit(0.0f, 127.0f, jmap(log10(jlimit(20.0f, 20000.0f, res[0])), log10(20.0f), log10(20000.0f), 0.0f, 127.0f)));
        auto spreadValue = roundToInt(jlimit(0.0f, 127.0f, jmap(log10(jlimit(20.0f, 20000.0f, res[1])), log10(20.0f), log10(20000.0f), 0.0f, 127.0f)));
        //mapping basato su osservazione perch non c' un range ben definito...
        auto skewValue = roundToInt(jlimit(0.0f, 127.0f, jmap(res[2], 0.0f, MAX_SKEW_THRESHOLD, 0.0f, 127.0f)));
        auto kurtosisValue = roundToInt(jlimit(0.0f, 127.0f, jmap(log10(jlimit(1.0f, MAX_KURT_THRESHOLD, res[3])), log10(1.0f), log10(MAX_KURT_THRESHOLD), 0.0f, 127.0f)));

        midiMessages.addEvent(MidiMessage::controllerEvent(MIDI_CHAN, CC_MOMENTS, centroidValue), 0);
        midiMessages.addEvent(MidiMessage::controllerEvent(MIDI_CHAN, CC_MOMENTS +1, spreadValue), 0);
        midiMessages.addEvent(MidiMessage::controllerEvent(MIDI_CHAN, CC_MOMENTS +2, skewValue), 0);
        midiMessages.addEvent(MidiMessage::controllerEvent(MIDI_CHAN, CC_MOMENTS +3 , kurtosisValue), 0);
    }
    else if (name == "Fundamental frequency") {
        if (res[0] > 20.0f) {

            float rawMidiNote = 12.0f * log2(res[0] / 440.0f) + 69.0f;
            int note = roundToInt(jlimit(0.0f, 127.0f, rawMidiNote));
            if (note != lastNote) {
                if (lastNote != -1) { midiMessages.addEvent(MidiMessage::noteOff(MIDI_CHAN, lastNote), 0); }
                midiMessages.addEvent(MidiMessage::noteOn(MIDI_CHAN, note, (uint8)100), 0);
                lastNote = note;
            }
        }
    }
    else if (name == "Chromagram") {
        std::vector<std::pair<float, int>> chroma;
        for (int i = 0; i < res.size(); ++i) {
            chroma.push_back({ res[i], i });
        }

        std::sort(chroma.begin(), chroma.end(), [](const std::pair<float, int>& a, const std::pair<float, int>& b) {
            return a.first > b.first;
            });

        std::vector<int> nextChord;
        for (int i = 0; i < CHORD_NOTE_AMT && i < chroma.size(); ++i) {
            if (chroma[i].first > 0.1f) {
                nextChord.push_back((DEFAULT_OCTAVE * 12) + chroma[i].second);
            }
        }

        for (int oldNote : lastChord) {

            if (std::find(nextChord.begin(), nextChord.end(), oldNote) == nextChord.end()) {
                midiMessages.addEvent(MidiMessage::noteOff(MIDI_CHAN, oldNote), 0);
            }
        }

        for (int newNote : nextChord) {

            if (std::find(lastChord.begin(), lastChord.end(), newNote) == lastChord.end()) {
                midiMessages.addEvent(MidiMessage::noteOn(MIDI_CHAN, newNote, (uint8)100), 0);
            }
        }

        lastChord = nextChord;
    }
}

void OscMapper::toOsc(const std::vector<float>& res, String name, OSCSender& sender) {
    if (res.empty()) return;
    String address = "/feature/" + name.replace(" ", "_");
    OSCMessage msg{ OSCAddressPattern(address) };

    for (float val : res) {
        msg.addFloat32(val);
    }

    sender.send(msg);
}
