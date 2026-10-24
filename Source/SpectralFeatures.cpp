#include "SpectralFeatures.h"

//TODO: fare diventare threshold macro

void SpectralMoments::calculateSpectralFeatures(const std::vector<float>& magnitudes, int fftSize, int numBins) {
    const float binToHz = static_cast<float>(sampleRate) / static_cast<float>(fftSize);

    float sumMag = 0.0f;
    float weightedSum = 0.0f;

    for (int i = 0; i < numBins; ++i)
    {
        float magnitude = magnitudes[i];
        float freq = i * binToHz;

        sumMag += magnitude;
        weightedSum += magnitude * freq;
    }

    if (sumMag > SILENCE_THRESH_SOFT) 
    {
        centroid = weightedSum / sumMag;

        float sumSquaredDiffs = 0.0f;
        float sumCubedDiffs = 0.0f;
        float sumFourthDiffs = 0.0f;

        for (int i = 0; i < numBins; ++i)
        {
            float freq = i * binToHz;
            float diff = freq - centroid;

            sumSquaredDiffs += (diff * diff) * magnitudes[i];
            sumCubedDiffs += (diff * diff * diff) * magnitudes[i];
            sumFourthDiffs += (diff * diff * diff * diff) * magnitudes[i];
        }

        spread = std::sqrt(sumSquaredDiffs / sumMag);
        skewness = static_cast<float>((sumCubedDiffs / sumMag) / std::pow(spread, 3.0));
        kurtosis = static_cast<float>((sumFourthDiffs / sumMag) / std::pow(spread, 4.0));
    }
    else
    {
        centroid = 0.0f;
        spread = 0.0f;
        skewness = 0.0f;
        kurtosis = 0.0f;
    }

    moments = { centroid, spread, skewness, kurtosis };
}

double F0::hzToErb(double hz)
{
    return 21.4 * std::log10(1.0 + hz / 229.0);
}

double F0::erbToHz(double erb)
{
    return (std::pow(10.0, erb / 21.4) - 1.0) * 229.0;
}

std::vector<int> F0::sieve(int n)
{
    if (n < 2)
        return std::vector<int>();

    std::vector<bool> isPrime(n + 1, true);
    isPrime[0] = isPrime[1] = false;

    for (int i = 2; i * i <= n; ++i)
    {
        if (isPrime[i])
        {
            for (int j = i * i; j <= n; j += i)
                isPrime[j] = false;
        }
    }

    std::vector<int> primes;
    for (int i = 2; i <= n; ++i)
    {
        if (isPrime[i])
            primes.push_back(i);
    }

    return primes;
}

void F0::generatePitchCandidates()
{
    mPitchCandidates.clear();

    double minErb = hzToErb(mMinFreq);
    double maxErb = hzToErb(mMaxFreq);
    int numCandidates = 100;

    for (int i = 0; i < numCandidates; ++i)
    {
        double erb = minErb + (maxErb - minErb) * i / (numCandidates - 1.0);
        mPitchCandidates.push_back(erbToHz(erb));
    }
}

void F0::generatePrimes(int maxHarmonic)
{
    mPrimes.clear();
    // Fondamentale = 1
    mPrimes.push_back(1);

    // Aggiungi altri primi
    std::vector<int> sievePrimes = sieve(std::min(maxHarmonic, 97));
    mPrimes.insert(mPrimes.end(), sievePrimes.begin(), sievePrimes.end());
}

void F0::setupAlgorithmParameters() {
    generatePitchCandidates();

    double minErb = hzToErb(mMinFreq / 2.0);
    double maxErb = hzToErb(mSampleRate / 2.0);
    int numErbChannels = 48;

    mErbFrequencies.clear();
    for (int i = 0; i < numErbChannels; ++i)
    {
        double erb = minErb + (maxErb - minErb) * i / (numErbChannels - 1.0);
        mErbFrequencies.push_back(erbToHz(erb));
    }

    int maxHarmonic = (int)(mSampleRate / (2.0 * mMinFreq));
    generatePrimes(maxHarmonic);

    mLoudnessMatrix.resize(mPitchCandidates.size());
    for (auto& row : mLoudnessMatrix)
        row.resize(mPrimes.size(), 0.0f);

    mStrengthVector.resize(mPitchCandidates.size(), 0.0f);
}

float F0::interpolatePeak(const std::vector<float>& data, int peakIndex)
{
    if (peakIndex <= 0 || peakIndex >= (int)data.size() - 1)
        return (float)peakIndex;

    float y1 = data[peakIndex - 1];
    float y2 = data[peakIndex];
    float y3 = data[peakIndex + 1];

    float denominator = y1 - 2.0f * y2 + y3;
    if (std::abs(denominator) < 1e-6f)
        return (float)peakIndex;

    float offset = 0.5f * (y1 - y3) / denominator;

    return peakIndex + offset;
}

void F0::calculateSpectralFeatures(const std::vector<float>& magnitudes, int fftSize, int numBins) {
    const double hzToBin = (double)fftSize / mSampleRate;

    for (size_t pitchIdx = 0; pitchIdx < mPitchCandidates.size(); ++pitchIdx)
    {
        double f0 = mPitchCandidates[pitchIdx];

        for (size_t primeIdx = 0; primeIdx < mPrimes.size(); ++primeIdx)
        {
            double harmonicFreq = f0 * mPrimes[primeIdx];

            if (harmonicFreq > mSampleRate / 2.0)
            {
                mLoudnessMatrix[pitchIdx][primeIdx] = 0.0f;
                continue;
            }

            int binIndex = static_cast<int>(std::round(harmonicFreq * hzToBin));

            if (binIndex >= 0 && binIndex < numBins)
            {
                mLoudnessMatrix[pitchIdx][primeIdx] = magnitudes[binIndex];
            }
            else
            {
                mLoudnessMatrix[pitchIdx][primeIdx] = 0.0f;
            }
        }
    }

    calculateStrength();
    findPitch();

    f0[0] = mCurrentPitchHz;
}

void F0::findPitch()
{
    auto maxIt = std::max_element(mStrengthVector.begin(), mStrengthVector.end());

    if (maxIt == mStrengthVector.end() || *maxIt < mStrengthThreshold)
    {
        mCurrentPitchHz = 0.0f;
        mCurrentStrength = 0.0f;
        return;
    }

    int peakIndex = (int)std::distance(mStrengthVector.begin(), maxIt);
    mCurrentStrength = *maxIt;

    // CONTROLLO ANTI-SUBARMONICA RAFFORZATO
    // Verifica ottave superiori (2x, 3x, 4x)
    float candidateFreq = (float)mPitchCandidates[peakIndex];

    // Controlla se esistono multipli (armoniche intere) con strength simile
    for (int multiplier = 2; multiplier <= 4; ++multiplier)
    {
        float targetFreq = candidateFreq * multiplier;

        if (targetFreq > mMaxFreq)
            break;

        // Cerca il candidato pi� vicino a targetFreq
        float bestDist = 1000.0f;
        int bestIdx = -1;

        for (size_t i = peakIndex + 1; i < mPitchCandidates.size(); ++i)
        {
            float testFreq = (float)mPitchCandidates[i];
            float dist = std::abs(testFreq - targetFreq);

            if (dist < bestDist && dist < 30.0f) // Tolleranza 30 Hz
            {
                bestDist = dist;
                bestIdx = (int)i;
            }

            if (testFreq > targetFreq + 50.0f)
                break;
        }

        // Se troviamo un multiplo con strength maggiore del 80%, � probabile sia la vera fondamentale
        if (bestIdx >= 0 && mStrengthVector[bestIdx] > *maxIt * 0.8f)
        {
            peakIndex = bestIdx;
            mCurrentStrength = mStrengthVector[bestIdx];
            candidateFreq = (float)mPitchCandidates[bestIdx];
            // Continua a cercare multipli ancora pi� alti
        }
    }

    // Interpolazione parabolica
    float interpolatedPitch = interpolatePeak(mStrengthVector, peakIndex);

    if (interpolatedPitch >= 0 && interpolatedPitch < (float)mPitchCandidates.size())
    {
        int idx = (int)interpolatedPitch;
        float frac = interpolatedPitch - idx;

        if (idx + 1 < (int)mPitchCandidates.size())
            mCurrentPitchHz = (float)(mPitchCandidates[idx] * (1.0 - frac) +
                mPitchCandidates[idx + 1] * frac);
        else
            mCurrentPitchHz = (float)mPitchCandidates[idx];
    }
    else
    {
        mCurrentPitchHz = candidateFreq;
    }
}

void F0::calculateStrength() {

    for (size_t pitchIdx = 0; pitchIdx < mPitchCandidates.size(); ++pitchIdx)
    {
        float weightedSum = 0.0f;

        for (size_t primeIdx = 0; primeIdx < mPrimes.size(); ++primeIdx)
        {
            float loudness = mLoudnessMatrix[pitchIdx][primeIdx];

            if (loudness > 0.0f)
            {
                // Peso MOLTO pi� forte sulla fondamentale
                float weight = primeIdx == 0 ? 3.0f : 1.0f / (1.0f + primeIdx * 0.6f);
                weightedSum += loudness * weight;
            }
        }

        // CRITICAL: Usa SOMMA ponderata, NON media ponderata
        // Questo permette a 440Hz di dominare su 220Hz
        mStrengthVector[pitchIdx] = weightedSum;
    }

    // Normalizza (necessario per comparare con threshold)
    float maxStrength = *std::max_element(mStrengthVector.begin(), mStrengthVector.end());

    //DBG("DEBUG Strength: maxStrength BEFORE norm = " + juce::String(maxStrength));
    //DBG("DEBUG Strength threshold = " + juce::String(mStrengthThreshold));

    if (maxStrength > 0.0f)
    {
        for (auto& s : mStrengthVector)
            s /= maxStrength;
    }
}

void Chromagram::prepareToPlay(double sr, int samplesPerBlock) {
    SpectralFeature::prepareToPlay(sr, samplesPerBlock);
    chroma = std::vector<float>(12, 0.0f);
}

void Chromagram::calculateSpectralFeatures(const std::vector<float>& magnitudes, int fftSize, int numBins) {
    std::fill(chroma.begin(), chroma.end(), 0.0f);
    const float binToHz = static_cast<float>(sampleRate) / static_cast<float>(fftSize); //converto bin in hz

    for (int i = 1; i < numBins - 2; ++i) {
        //prendere solo picchi locali (i valori più grandi sia bin prec che succ)
        //non capisco se sia meglio...
        if (magnitudes[i] > magnitudes[i - 1] && magnitudes[i] > magnitudes[i + 1]) {
            float freq = i * binToHz;
            if (freq >= 32.7f)  { //usato minimo essentia
                float midiNote = 69.0f + 12.0f * std::log2(freq / 440.0f);

                int noteNum = roundToInt(midiNote);
                int chromaBin = ((noteNum % 12) + 12) % 12;

                chroma[chromaBin] += magnitudes[i];
            }
        }
    }

    float totalEnergy = 0.0f;
    for (float v : chroma) totalEnergy += v;

    if (totalEnergy > (SILENCE_THRESH_HARD)) {
        for (float& v : chroma) v /= totalEnergy;
    }
}