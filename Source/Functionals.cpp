#include "Functionals.h"
#include <algorithm>

void Average::store(const float* values, int numValues) {
    
    if (numValues == 0) return;

    if (values[0] <= SILENCE_THRESH_SOFT) return;

    for (int i = 0; i < numValues; ++i) {
        sums[i] += static_cast<double>(values[i]);
    }

    ++count;
}

std::vector<float> Average::getResult()  {
    std::vector<float> res;
    if (count == 0 || sums.empty())
        return res;

    for (int i = 0; i < n; ++i) {

        res.push_back(static_cast<float>(sums[i] / count));
    }

    return res;
}

void Median::store(const float* values, int numValues)
{
    if (numValues == 0) return;


    for (int i = 0; i < numValues; ++i) {
        samples[i].push_back(values[i]);
    }
}

std::vector<float> Median::getResult() {
    std::vector<float> res;
    if (samples.empty() || samples[0].empty()) return res;

    for (int i = 0; i < n; ++i) {
        auto& v = samples[i];
        int n = static_cast<int>(v.size() / 2);
        std::nth_element(v.begin(), v.begin() + n, v.end());
        float medianVal = v[n];

        if (v.size() % 2 == 0 && n > 0) { // mediana su size pari (media tra valori centrali)
            float low = *std::max_element(v.begin(), v.begin() + n);
            medianVal = (low + medianVal) / 2.f;
        }
        res.push_back(medianVal);
    }
    return res;
}

void StdDev::store(const float* values, int numValues) {

    if (numValues == 0) return;

    for (int i = 0; i < numValues; ++i) {
        double val = static_cast<double>(values[i]);
        sums[i] += val;
        sumSquares[i] += (val * val);
    }
    ++count;
}

std::vector<float> StdDev::getResult() {
    std::vector<float> res;
    if (count < 2) return res;

    for (int i = 0; i < n; ++i) {
        double mean = sums[i] / count;
        double variance = (sumSquares[i] / count) - (mean * mean);
        res.push_back(static_cast<float>(std::sqrt(std::max(0.0, variance))));
    }
    return res;
}

void IQR::store(const float* values, int numValues) {

    if (numValues == 0) return;

    for (int i = 0; i < numValues; ++i) {
        samples[i].push_back(values[i]);
    }
}

std::vector<float> IQR::getResult() {
    std::vector<float> res;
    if (samples.empty() || samples[0].empty()) return res;

    for (int i = 0; i < n; ++i) {
        auto& v = samples[i];

        if (v.size() < 4) {
            continue; 
        }

        sort(v.begin(), v.end());
        std::vector<float> q1, q3;

        for (int j = 0; j < v.size() / 2; ++j) { q1.push_back(v[j]); }
        for (int j = static_cast<int>(v.size()/2) + 1;j < v.size(); ++j){q3.push_back(v[j]);}

        res.push_back(q3[q3.size() / 2] - q1[q1.size() / 2]);
    }
    return res;
}

void Max::store(const float* values, int numValues) {
    if (numValues == 0) return;

    for (int i = 0; i < numValues; ++i) {
        maxes[i] = jmax(maxes[i], values[i]);

    }
}

std::vector<float> Max::getResult() {
    std::vector<float> res;
    if (maxes.empty()) return res;
    return maxes;
}

void Min::store(const float* values, int numValues) {
    if (numValues == 0) return;

    for (int i = 0; i < numValues; ++i) {
        mins[i] = jmin(mins[i], values[i]);
    }
}

std::vector<float> Min::getResult() {
    std::vector<float> res;
    if (mins.empty()) return res;
    return mins;

}
