#include "Functionals.h"
#include <algorithm>

void Average::store(const FeatureResult& res) {
    
    if (res.values.empty()) return;

    if (sums.empty()) {
        sums.resize(res.values.size(), 0.0);
        savedNames = res.names;
    }

    if (res.values[0] <= SILENCE_THRESH_SOFT) return;

    for (int i = 0; i < res.values.size(); ++i) {
        sums[i] += (double)(res.values[i]);
    }

    ++count;
}

FeatureResult Average::getResult()  {
    FeatureResult res;
    if (count == 0 || sums.empty())
        return res;

    for (int i = 0; i < sums.size(); ++i) {
        float finalAverage = (float)(sums[i] / count);

        String label = savedNames[i];
        res.add(label, finalAverage);
    }

    return res;
}

void Median::store(const FeatureResult& res)
{
    if (res.values.empty()) return;

    if (values.empty()) {
        values.resize(res.values.size());
        savedNames = res.names;
    }

    for (int i = 0; i < res.values.size(); ++i) {
        values[i].push_back(res.values[i]);
    }
}

FeatureResult Median::getResult() {
    FeatureResult res;
    if (values.empty() || values[0].empty()) return res;

    for (int i = 0; i < values.size(); ++i) {
        auto& v = values[i];
        int n = static_cast<int>(v.size() / 2);
        std::nth_element(v.begin(), v.begin() + n, v.end());
        float medianVal = v[n];

        if (v.size() % 2 == 0 && n > 0) { // mediana su size pari (media tra valori centrali)
            float low = *std::max_element(v.begin(), v.begin() + n);
            medianVal = (low + medianVal) / 2.f;
        }
        res.add(savedNames[i], medianVal);
    }
    return res;
}

void StdDev::store(const FeatureResult& res) {
    if (sums.empty()) {
        sums.resize(res.values.size(), 0.0);
        sumSquares.resize(res.values.size(), 0.0);
        savedNames = res.names;
    }
    for (int i = 0; i < res.values.size(); ++i) {
        double val = static_cast<double>(res.values[i]);
        sums[i] += val;
        sumSquares[i] += (val * val);
    }
    ++count;
}

FeatureResult StdDev::getResult() {
    FeatureResult res;
    if (count < 2) return res;

    for (int i = 0; i < sums.size(); ++i) {
        double mean = sums[i] / count;
        
        double variance = (sumSquares[i] / count) - (mean * mean);
        res.add(savedNames[i], static_cast<float>(std::sqrt(std::max(0.0, variance))));
    }
    return res;
}

void IQR::store(const FeatureResult& res) {

    if (res.values.empty()) return;

    if (values.empty()) {
        values.resize(res.values.size());
        savedNames = res.names;
    }

    for (int i = 0; i < res.values.size(); ++i) {
        values[i].push_back(res.values[i]);
    }
}

FeatureResult IQR::getResult() {
    FeatureResult res;
    if (values.empty() || values[0].empty()) return res;

    for (int i = 0; i < values.size(); ++i) {
        auto& v = values[i];

        if (v.size() < 4) {
            continue; 
        }

        sort(v.begin(), v.end());
        std::vector<float> q1, q3;

        for (int j = 0; j < v.size() / 2; ++j) { q1.push_back(v[j]); }
        for (int j = static_cast<int>(v.size()/2) + 1;j < v.size(); ++j){q3.push_back(v[j]);}

        auto iqr = q3[q3.size()/2] - q1[q1.size() / 2];
        res.add(savedNames[i], iqr);
    }
    return res;
}

void Max::store(const FeatureResult& res) {
    if (res.values.empty()) return;

    if (maxes.empty()) {
        maxes.resize(res.values.size());
        std::fill(maxes.begin(), maxes.end(), std::numeric_limits<float>::lowest());
        savedNames = res.names;
    }

    for (int i = 0; i < res.values.size(); ++i) {
        maxes[i] = jmax(maxes[i], res.values[i]);

    }
}

FeatureResult Max::getResult() {
    FeatureResult res;
    if (maxes.empty()) return res;

    for (int i = 0; i < maxes.size(); ++i) {
        res.add(savedNames[i], maxes[i]);
    }
    return res;
}

void Min::store(const FeatureResult& res) {
    if (res.values.empty()) return; 

    if (mins.empty()) {
        mins.resize(res.values.size());
        std::fill(mins.begin(), mins.end(), std::numeric_limits<float>::max());

        savedNames = res.names;
    }

    for (int i = 0; i < res.values.size(); ++i) {
        mins[i] = jmin(mins[i], res.values[i]);

    }
}

FeatureResult Min::getResult() {
    FeatureResult res;
    if (mins.empty()) return res;

    for (int i = 0; i < mins.size(); ++i) {
        res.add(savedNames[i], mins[i]);
    }
    return res;

}
