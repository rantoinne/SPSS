#pragma once

#include "spss/DatasetStore.hpp"
#include "spss/Output.hpp"
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>

namespace spss {
namespace stats {

inline std::string fmt(double v, int d = 4) {
    if (std::isnan(v)) return ".";
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(d) << v;
    return oss.str();
}

inline PivotCell cell(double v, int d = 4) {
    PivotCell c;
    c.value = v;
    c.isMissing = std::isnan(v);
    c.display = fmt(v, d);
    return c;
}

struct WeightedSample {
    std::vector<double> x;
    std::vector<double> w;
    double n{0};
    double sum{0};
    double mean{0};
    double var{0}; // sample variance with weight
    double sd{0};
    double min{0};
    double max{0};

    static WeightedSample fromVar(const DatasetStore& ds, size_t col) {
        WeightedSample s;
        bool first = true;
        for (size_t r = 0; r < ds.caseCount(); ++r) {
            if (!ds.caseSelected(r) || ds.isMissing(r, col)) continue;
            double v = ds.getNumeric(r, col);
            double wt = ds.caseWeight(r);
            if (wt <= 0) continue;
            s.x.push_back(v);
            s.w.push_back(wt);
            s.sum += v * wt;
            s.n += wt;
            if (first || v < s.min) s.min = v;
            if (first || v > s.max) s.max = v;
            first = false;
        }
        if (s.n > 0) s.mean = s.sum / s.n;
        double ss = 0;
        for (size_t i = 0; i < s.x.size(); ++i) {
            double d = s.x[i] - s.mean;
            ss += s.w[i] * d * d;
        }
        if (s.n > 1) {
            s.var = ss / (s.n - 1.0);
            s.sd = std::sqrt(s.var);
        }
        return s;
    }
};

inline double erfApprox(double x) {
    // Abramowitz & Stegun
    double t = 1.0 / (1.0 + 0.3275911 * std::fabs(x));
    double a1 = 0.254829592, a2 = -0.284496736, a3 = 1.421413741, a4 = -1.453152027, a5 = 1.061405429;
    double y = 1.0 - (((((a5 * t + a4) * t) + a3) * t + a2) * t + a1) * t * std::exp(-x * x);
    return x >= 0 ? y : -y;
}

inline double normCdf(double z) {
    return 0.5 * (1.0 + erfApprox(z / std::sqrt(2.0)));
}

inline double studentPTwoTailed(double t, double df) {
    // rough approximation via normal for large df; beta incomplete for small
    if (df <= 0 || std::isnan(t)) return std::numeric_limits<double>::quiet_NaN();
    double z = std::fabs(t);
    if (df > 30) return 2.0 * (1.0 - normCdf(z));
    // Cornish-ish: convert t to approx normal
    double p = 2.0 * (1.0 - normCdf(z * (1.0 - 1.0 / (4.0 * df)) / std::sqrt(1.0 + z * z / (2.0 * df))));
    return std::clamp(p, 0.0, 1.0);
}

inline double pearson(const std::vector<double>& x, const std::vector<double>& y,
                      const std::vector<double>& w) {
    double n = 0, sx = 0, sy = 0;
    for (size_t i = 0; i < x.size(); ++i) {
        n += w[i]; sx += w[i] * x[i]; sy += w[i] * y[i];
    }
    if (n < 2) return std::numeric_limits<double>::quiet_NaN();
    double mx = sx / n, my = sy / n;
    double sxx = 0, syy = 0, sxy = 0;
    for (size_t i = 0; i < x.size(); ++i) {
        double dx = x[i] - mx, dy = y[i] - my;
        sxx += w[i] * dx * dx;
        syy += w[i] * dy * dy;
        sxy += w[i] * dx * dy;
    }
    if (sxx <= 0 || syy <= 0) return std::numeric_limits<double>::quiet_NaN();
    return sxy / std::sqrt(sxx * syy);
}

inline double chiSquareP(double chi2, int df) {
    // Wilson-Hilferty approximation for chi-square CDF upper tail
    if (df <= 0 || chi2 < 0 || std::isnan(chi2)) return std::numeric_limits<double>::quiet_NaN();
    double h = std::pow(chi2 / df, 1.0 / 3.0);
    double mu = 1.0 - 2.0 / (9.0 * df);
    double sigma = std::sqrt(2.0 / (9.0 * df));
    double z = (h - mu) / sigma;
    return 1.0 - normCdf(z);
}

} // namespace stats
} // namespace spss
