#include "TemporalFeatures.h"


void RRMS::prepareToPlay(double sr, int samplesPerBlock)
{
    sampleRate = sr;
    history.setSize(1, roundToInt(maxTime * sampleRate));
    aux.setSize(1, samplesPerBlock);
    reset();
}

void RRMS::releaseResources()
{
    history.setSize(0, 0);
    aux.setSize(0, 0);
}

void RRMS::processBlock(AudioBuffer<float>& buffer)
{
    jassert(windowSize > 0 && "RRMS::processBlock chiamato senza prepareToPlay()");
    if (windowSize == 0) return;

    const auto numSamples = buffer.getNumSamples();
    const auto numCh = buffer.getNumChannels();

    if (numCh == 2) {
        aux.copyFrom(0, 0, buffer, 0, 0, numSamples);
        aux.addFrom(0, 0, buffer, 1, 0, numSamples);
        aux.applyGain(1.0f / MathConstants<float>::sqrt2);
    }
    else {
        aux.copyFrom(0, 0, buffer, 0, 0, numSamples);
    }

    auto auxData = aux.getWritePointer(0);
    auto historyData = history.getWritePointer(0);

    double sumRms = 0.;
    for (int smp = 0; smp < numSamples; ++smp) {
        float squareSample = auxData[smp] * auxData[smp];

        avg += squareSample - historyData[historyIndex];
        historyData[historyIndex] = squareSample;

        if (avg < 0.0f) { avg = 0.0f; }

        auxData[smp] = std::sqrt(avg / static_cast<float>(windowSize));
        sumRms += auxData[smp];

        ++historyIndex %= windowSize;
    }

    rmsValue = static_cast<float>(sumRms / numSamples);
}

void RRMS::reset()
{
    windowSize = jlimit(1, roundToInt(maxTime * sampleRate), roundToInt(rmsTime * sampleRate));
    history.clear();
    aux.clear();
    historyIndex = 0;
    avg = 0.0;
}

void PAN::processBlock(AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2)
    {
        panValue = 0.0f;
        return;
    }

    const int numSamples = buffer.getNumSamples();
    const float* L = buffer.getReadPointer(0);
    const float* R = buffer.getReadPointer(1);

    float lEnergy = 0.0f;
    float rEnergy = 0.0f;

    for (int i = 0; i < numSamples; ++i) {
        lEnergy += L[i] * L[i];
        rEnergy += R[i] * R[i];
    }

    lEnergy /= numSamples;
    rEnergy /= numSamples;

    const float totalEnergy = lEnergy + rEnergy;

    if (totalEnergy < SILENCE_THRESH_SOFT) {
        panValue = 0.0f;
    }
    else {
        panValue = (rEnergy - lEnergy) / totalEnergy;
    }
}

void Brightness::prepareToPlay(double sr, int samplesPerBlock)
{
    envelope.prepareToPlay(sr, samplesPerBlock);
    filterEnvelope.prepareToPlay(sr, samplesPerBlock);
    filterBuffer.setSize(1, samplesPerBlock);
    filterBuffer.clear();
    sampleRate = sr;
    lastSample = 0.0f;
}

void Brightness::releaseResources()
{
    envelope.releaseResources();
    filterEnvelope.releaseResources();
    filterBuffer.setSize(0, 0);
}

void Brightness::processBlock(AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numCh = buffer.getNumChannels();

    if (numCh == 2) {
        filterBuffer.copyFrom(0, 0, buffer, 0, 0, numSamples);
        filterBuffer.addFrom(0, 0, buffer, 1, 0, numSamples);
        filterBuffer.applyGain(1 / MathConstants<float>::sqrt2);
    }
    else {
        filterBuffer.copyFrom(0, 0, buffer, 0, 0, numSamples);
    }

    auto data = filterBuffer.getWritePointer(0);

    for (auto smp = 0; smp < numSamples; ++smp) {
        float currentSample = data[smp];
        data[smp] = currentSample - lastSample;
        lastSample = currentSample;
    }

    envelope.processBlock(buffer);
    filterEnvelope.processBlock(filterBuffer);

    FeatureResult envData, fEnvData;
    envelope.getResult(envData);
    filterEnvelope.getResult(fEnvData);

    float envVal = envData.values.empty() ? 0.0f : envData.values[0];
    float fEnvVal = fEnvData.values.empty() ? 0.0f : fEnvData.values[0];

    float b = 0.0f;

    if (envVal < SILENCE_THRESH_SOFT) {
        b = 0.0f;
        lastSample = 0.0f; 
    }
    else {
        b = fEnvVal / envVal;
    }

    b = jlimit(0.0f, 2.0f, b);
    brightValue = static_cast<float>((sampleRate / MathConstants<float>::pi) * (asin(b / 2.0f)));
}

void ZCR::processBlock(AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numCh = buffer.getNumChannels();

    int crossCount = 0;

    for (int i = 0; i < numSamples; ++i) {

        float smp = 0.0f;

        for (int ch = 0; ch < numCh; ++ch) {

            smp += buffer.getSample(ch, i);

        }

        smp /= numCh;

        if (smp > SILENCE_THRESH_HARD) {
            
            if (lastSign < 0) {
                crossCount++;
            }

            lastSign = 1;
        }

        else if (smp < -SILENCE_THRESH_HARD) {
            
            if (lastSign > 0) {
                crossCount++;
            }

            lastSign = -1;
        
        }

    }

    zcrValue = (float)crossCount / numSamples;
}