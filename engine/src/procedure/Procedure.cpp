#include "spss/Procedure.hpp"
#include "StatsUtil.hpp"
#include <map>
#include <set>
#include <cmath>
#include <numeric>
#include <limits>

namespace spss {

OutputDocument ProcedureRunner::frequencies(const DatasetStore& ds, const FrequenciesOptions& opt) {
    OutputDocument doc;
    auto logIdx = doc.addItem(OutputLogEntry{"FREQUENCIES", "Running FREQUENCIES", false});
    doc.addToOutline("Log", logIdx, "FREQUENCIES");

    for (const auto& name : opt.variables) {
        int col = ds.indexOf(name);
        if (col < 0) {
            auto w = doc.addItem(OutputWarning{"Variable not found: " + name});
            doc.addToOutline("Warnings", w, name);
            continue;
        }
        std::map<double, double> counts;
        double total = 0, missing = 0;
        for (size_t r = 0; r < ds.caseCount(); ++r) {
            if (!ds.caseSelected(r)) continue;
            double w = ds.caseWeight(r);
            if (ds.isMissing(r, static_cast<size_t>(col))) { missing += w; continue; }
            double v = ds.getNumeric(r, static_cast<size_t>(col));
            counts[v] += w;
            total += w;
        }
        PivotTable t;
        t.title = "Frequencies: " + name;
        t.columnLabels = {"Frequency", "Percent", "Valid Percent", "Cumulative Percent"};
        double cum = 0;
        for (const auto& [val, freq] : counts) {
            cum += freq;
            t.rowLabels.push_back(stats::fmt(val, 2));
            double pct = total + missing > 0 ? 100.0 * freq / (total + missing) : 0;
            double vp = total > 0 ? 100.0 * freq / total : 0;
            double cp = total > 0 ? 100.0 * cum / total : 0;
            t.cells.push_back({stats::cell(freq, 1), stats::cell(pct, 1), stats::cell(vp, 1), stats::cell(cp, 1)});
        }
        t.rowLabels.push_back("Total");
        t.cells.push_back({stats::cell(total, 1), stats::cell(100, 1), stats::cell(100, 1), stats::cell(100, 1)});
        if (missing > 0) t.footnotes.push_back("Missing: " + stats::fmt(missing, 1));
        auto ti = doc.addItem(t);
        doc.addToOutline("Tables", ti, t.title);

        if (opt.barchart) {
            ChartSpec ch;
            ch.title = "Bar Chart: " + name;
            ch.type = "bar";
            ch.xLabel = name;
            ch.yLabel = "Frequency";
            for (const auto& [val, freq] : counts) {
                ch.categories.push_back(stats::fmt(val, 2));
                ch.values.push_back(freq);
            }
            auto ci = doc.addItem(ch);
            doc.addToOutline("Charts", ci, ch.title);
        }
    }
    return doc;
}

OutputDocument ProcedureRunner::descriptives(const DatasetStore& ds, const DescriptivesOptions& opt) {
    OutputDocument doc;
    auto logIdx = doc.addItem(OutputLogEntry{"DESCRIPTIVES", "Running DESCRIPTIVES", false});
    doc.addToOutline("Log", logIdx, "DESCRIPTIVES");

    PivotTable t;
    t.title = "Descriptive Statistics";
    std::vector<std::string> cols;
    if (opt.n) cols.push_back("N");
    if (opt.min) cols.push_back("Minimum");
    if (opt.max) cols.push_back("Maximum");
    if (opt.mean) cols.push_back("Mean");
    if (opt.stddev) cols.push_back("Std. Deviation");
    t.columnLabels = cols;

    for (const auto& name : opt.variables) {
        int col = ds.indexOf(name);
        if (col < 0) continue;
        auto s = stats::WeightedSample::fromVar(ds, static_cast<size_t>(col));
        t.rowLabels.push_back(name);
        std::vector<PivotCell> row;
        if (opt.n) row.push_back(stats::cell(s.n, 0));
        if (opt.min) row.push_back(stats::cell(s.n > 0 ? s.min : NAN));
        if (opt.max) row.push_back(stats::cell(s.n > 0 ? s.max : NAN));
        if (opt.mean) row.push_back(stats::cell(s.n > 0 ? s.mean : NAN));
        if (opt.stddev) row.push_back(stats::cell(s.n > 1 ? s.sd : NAN));
        t.cells.push_back(row);
    }
    auto ti = doc.addItem(t);
    doc.addToOutline("Tables", ti, t.title);
    return doc;
}

OutputDocument ProcedureRunner::explore(const DatasetStore& ds, const ExploreOptions& opt) {
    OutputDocument doc;
    DescriptivesOptions d;
    d.variables = opt.dependents;
    d.kurtosis = true;
    d.skewness = true;
    doc.append(descriptives(ds, d));

    for (const auto& name : opt.dependents) {
        int col = ds.indexOf(name);
        if (col < 0) continue;
        auto s = stats::WeightedSample::fromVar(ds, static_cast<size_t>(col));
        ChartSpec ch;
        ch.title = "Histogram: " + name;
        ch.type = "histogram";
        ch.xLabel = name;
        ch.yLabel = "Frequency";
        if (s.n > 0) {
            int bins = std::max(5, std::min(20, static_cast<int>(std::sqrt(s.n))));
            double width = (s.max - s.min) / bins;
            if (width <= 0) width = 1;
            std::vector<double> counts(bins, 0);
            for (size_t i = 0; i < s.x.size(); ++i) {
                int b = static_cast<int>((s.x[i] - s.min) / width);
                if (b >= bins) b = bins - 1;
                if (b < 0) b = 0;
                counts[static_cast<size_t>(b)] += s.w[i];
            }
            for (int i = 0; i < bins; ++i) {
                ch.categories.push_back(stats::fmt(s.min + (i + 0.5) * width, 2));
                ch.values.push_back(counts[static_cast<size_t>(i)]);
            }
        }
        auto ci = doc.addItem(ch);
        doc.addToOutline("Charts", ci, ch.title);
    }
    return doc;
}

OutputDocument ProcedureRunner::crosstabs(const DatasetStore& ds, const CrosstabsOptions& opt) {
    OutputDocument doc;
    if (opt.rows.empty() || opt.cols.empty()) return doc;
    int rcol = ds.indexOf(opt.rows[0]);
    int ccol = ds.indexOf(opt.cols[0]);
    if (rcol < 0 || ccol < 0) {
        auto w = doc.addItem(OutputWarning{"Crosstabs variables not found"});
        doc.addToOutline("Warnings", w, "Crosstabs");
        return doc;
    }
    std::map<double, std::map<double, double>> table;
    std::set<double> rowLevels, colLevels;
    double grand = 0;
    for (size_t r = 0; r < ds.caseCount(); ++r) {
        if (!ds.caseSelected(r)) continue;
        if (ds.isMissing(r, static_cast<size_t>(rcol)) || ds.isMissing(r, static_cast<size_t>(ccol))) continue;
        double rv = ds.getNumeric(r, static_cast<size_t>(rcol));
        double cv = ds.getNumeric(r, static_cast<size_t>(ccol));
        double w = ds.caseWeight(r);
        table[rv][cv] += w;
        rowLevels.insert(rv);
        colLevels.insert(cv);
        grand += w;
    }
    PivotTable t;
    t.title = "Crosstabulation: " + opt.rows[0] + " * " + opt.cols[0];
    for (double c : colLevels) t.columnLabels.push_back(stats::fmt(c, 2));
    t.columnLabels.push_back("Total");

    std::map<double, double> colTot;
    double chi = 0;
    int df = static_cast<int>((rowLevels.size() - 1) * (colLevels.size() - 1));

    for (double rv : rowLevels) {
        t.rowLabels.push_back(stats::fmt(rv, 2));
        std::vector<PivotCell> row;
        double rt = 0;
        for (double cv : colLevels) {
            double obs = table[rv][cv];
            rt += obs;
            colTot[cv] += obs;
            row.push_back(stats::cell(obs, 1));
        }
        row.push_back(stats::cell(rt, 1));
        t.cells.push_back(row);
    }
    t.rowLabels.push_back("Total");
    std::vector<PivotCell> totRow;
    for (double cv : colLevels) totRow.push_back(stats::cell(colTot[cv], 1));
    totRow.push_back(stats::cell(grand, 1));
    t.cells.push_back(totRow);

    if (opt.chiSquare && grand > 0 && df > 0) {
        for (double rv : rowLevels) {
            double rt = 0;
            for (double cv : colLevels) rt += table[rv][cv];
            for (double cv : colLevels) {
                double exp = rt * colTot[cv] / grand;
                if (exp > 0) {
                    double obs = table[rv][cv];
                    chi += (obs - exp) * (obs - exp) / exp;
                }
            }
        }
        t.footnotes.push_back("Pearson Chi-Square = " + stats::fmt(chi) +
                              "  df = " + std::to_string(df) +
                              "  Asymp. Sig. = " + stats::fmt(stats::chiSquareP(chi, df)));
    }
    auto ti = doc.addItem(t);
    doc.addToOutline("Tables", ti, t.title);
    return doc;
}

OutputDocument ProcedureRunner::ttest(const DatasetStore& ds, const TTestOptions& opt) {
    OutputDocument doc;
    PivotTable t;
    t.title = "One-Sample Test";
    t.columnLabels = {"N", "Mean", "Std. Deviation", "Std. Error Mean", "t", "df", "Sig. (2-tailed)", "Mean Difference"};

    if (opt.kind == TTestOptions::Kind::OneSample) {
        for (const auto& name : opt.variables) {
            int col = ds.indexOf(name);
            if (col < 0) continue;
            auto s = stats::WeightedSample::fromVar(ds, static_cast<size_t>(col));
            double se = s.n > 1 ? s.sd / std::sqrt(s.n) : NAN;
            double diff = s.mean - opt.testValue;
            double tv = se > 0 ? diff / se : NAN;
            double df = s.n - 1;
            double p = stats::studentPTwoTailed(tv, df);
            t.rowLabels.push_back(name);
            t.cells.push_back({
                stats::cell(s.n, 0), stats::cell(s.mean), stats::cell(s.sd), stats::cell(se),
                stats::cell(tv), stats::cell(df, 0), stats::cell(p), stats::cell(diff)
            });
        }
    } else if (opt.kind == TTestOptions::Kind::Independent) {
        t.title = "Independent Samples Test";
        int gcol = ds.indexOf(opt.groupVariable);
        if (gcol < 0 || opt.variables.empty()) return doc;
        int ycol = ds.indexOf(opt.variables[0]);
        if (ycol < 0) return doc;
        std::vector<double> x1, w1, x2, w2;
        for (size_t r = 0; r < ds.caseCount(); ++r) {
            if (!ds.caseSelected(r) || ds.isMissing(r, static_cast<size_t>(ycol)) ||
                ds.isMissing(r, static_cast<size_t>(gcol))) continue;
            double g = ds.getNumeric(r, static_cast<size_t>(gcol));
            double y = ds.getNumeric(r, static_cast<size_t>(ycol));
            double w = ds.caseWeight(r);
            if (g == opt.group1) { x1.push_back(y); w1.push_back(w); }
            else if (g == opt.group2) { x2.push_back(y); w2.push_back(w); }
        }
        auto mean = [](const std::vector<double>& x, const std::vector<double>& w) {
            double n = 0, s = 0; for (size_t i = 0; i < x.size(); ++i) { n += w[i]; s += w[i]*x[i]; }
            return std::make_pair(n, n > 0 ? s / n : NAN);
        };
        auto [n1, m1] = mean(x1, w1);
        auto [n2, m2] = mean(x2, w2);
        double ss1 = 0, ss2 = 0;
        for (size_t i = 0; i < x1.size(); ++i) ss1 += w1[i] * (x1[i]-m1)*(x1[i]-m1);
        for (size_t i = 0; i < x2.size(); ++i) ss2 += w2[i] * (x2[i]-m2)*(x2[i]-m2);
        double v1 = n1 > 1 ? ss1/(n1-1) : NAN;
        double v2 = n2 > 1 ? ss2/(n2-1) : NAN;
        double pooled = ((n1-1)*v1 + (n2-1)*v2) / (n1+n2-2);
        double se = std::sqrt(pooled * (1/n1 + 1/n2));
        double tv = se > 0 ? (m1-m2)/se : NAN;
        double df = n1+n2-2;
        double p = stats::studentPTwoTailed(tv, df);
        t.rowLabels.push_back(opt.variables[0]);
        t.columnLabels = {"N1", "N2", "Mean Diff", "t", "df", "Sig. (2-tailed)"};
        t.cells.push_back({
            stats::cell(n1,0), stats::cell(n2,0), stats::cell(m1-m2),
            stats::cell(tv), stats::cell(df,0), stats::cell(p)
        });
    } else {
        t.title = "Paired Samples Test";
        int c1 = ds.indexOf(opt.pairedVar1);
        int c2 = ds.indexOf(opt.pairedVar2);
        if (c1 < 0 || c2 < 0) return doc;
        std::vector<double> d, w;
        for (size_t r = 0; r < ds.caseCount(); ++r) {
            if (!ds.caseSelected(r)) continue;
            if (ds.isMissing(r, static_cast<size_t>(c1)) || ds.isMissing(r, static_cast<size_t>(c2))) continue;
            d.push_back(ds.getNumeric(r, static_cast<size_t>(c1)) - ds.getNumeric(r, static_cast<size_t>(c2)));
            w.push_back(ds.caseWeight(r));
        }
        double n = 0, sum = 0;
        for (size_t i = 0; i < d.size(); ++i) { n += w[i]; sum += w[i]*d[i]; }
        double mean = n > 0 ? sum/n : NAN;
        double ss = 0;
        for (size_t i = 0; i < d.size(); ++i) ss += w[i]*(d[i]-mean)*(d[i]-mean);
        double sd = n > 1 ? std::sqrt(ss/(n-1)) : NAN;
        double se = n > 0 ? sd/std::sqrt(n) : NAN;
        double tv = se > 0 ? mean/se : NAN;
        double df = n-1;
        t.rowLabels.push_back(opt.pairedVar1 + " - " + opt.pairedVar2);
        t.cells.push_back({
            stats::cell(n,0), stats::cell(mean), stats::cell(sd), stats::cell(se),
            stats::cell(tv), stats::cell(df,0), stats::cell(stats::studentPTwoTailed(tv, df)), stats::cell(mean)
        });
    }
    auto ti = doc.addItem(t);
    doc.addToOutline("Tables", ti, t.title);
    return doc;
}

OutputDocument ProcedureRunner::oneway(const DatasetStore& ds, const OnewayOptions& opt) {
    OutputDocument doc;
    int ycol = ds.indexOf(opt.dependent);
    int fcol = ds.indexOf(opt.factor);
    if (ycol < 0 || fcol < 0) return doc;

    std::map<double, std::vector<std::pair<double,double>>> groups;
    double grandN = 0, grandSum = 0;
    for (size_t r = 0; r < ds.caseCount(); ++r) {
        if (!ds.caseSelected(r)) continue;
        if (ds.isMissing(r, static_cast<size_t>(ycol)) || ds.isMissing(r, static_cast<size_t>(fcol))) continue;
        double y = ds.getNumeric(r, static_cast<size_t>(ycol));
        double f = ds.getNumeric(r, static_cast<size_t>(fcol));
        double w = ds.caseWeight(r);
        groups[f].push_back({y, w});
        grandN += w;
        grandSum += y * w;
    }
    double grandMean = grandN > 0 ? grandSum / grandN : NAN;
    double ssb = 0, ssw = 0;
    int k = 0;
    for (auto& [g, vals] : groups) {
        double n = 0, s = 0;
        for (auto& [y, w] : vals) { n += w; s += y * w; }
        double m = n > 0 ? s / n : 0;
        ssb += n * (m - grandMean) * (m - grandMean);
        for (auto& [y, w] : vals) ssw += w * (y - m) * (y - m);
        ++k;
    }
    double dfb = k - 1;
    double dfw = grandN - k;
    double msb = dfb > 0 ? ssb / dfb : NAN;
    double msw = dfw > 0 ? ssw / dfw : NAN;
    double F = msw > 0 ? msb / msw : NAN;
    // approx p via chi/normal for F — crude
    double p = stats::chiSquareP(F * dfb, static_cast<int>(dfb)); // rough stand-in

    PivotTable t;
    t.title = "ANOVA: " + opt.dependent + " by " + opt.factor;
    t.columnLabels = {"Sum of Squares", "df", "Mean Square", "F", "Sig."};
    t.rowLabels = {"Between Groups", "Within Groups", "Total"};
    t.cells = {
        {stats::cell(ssb), stats::cell(dfb,0), stats::cell(msb), stats::cell(F), stats::cell(p)},
        {stats::cell(ssw), stats::cell(dfw,0), stats::cell(msw), stats::cell(NAN), stats::cell(NAN)},
        {stats::cell(ssb+ssw), stats::cell(dfb+dfw,0), stats::cell(NAN), stats::cell(NAN), stats::cell(NAN)}
    };
    auto ti = doc.addItem(t);
    doc.addToOutline("Tables", ti, t.title);
    return doc;
}

OutputDocument ProcedureRunner::correlations(const DatasetStore& ds, const CorrelationsOptions& opt) {
    OutputDocument doc;
    size_t n = opt.variables.size();
    PivotTable t;
    t.title = "Correlations";
    for (const auto& v : opt.variables) t.columnLabels.push_back(v);
    for (size_t i = 0; i < n; ++i) {
        t.rowLabels.push_back(opt.variables[i]);
        std::vector<PivotCell> row;
        for (size_t j = 0; j < n; ++j) {
            if (i == j) { row.push_back(stats::cell(1.0)); continue; }
            int ci = ds.indexOf(opt.variables[i]);
            int cj = ds.indexOf(opt.variables[j]);
            std::vector<double> x, y, w;
            for (size_t r = 0; r < ds.caseCount(); ++r) {
                if (!ds.caseSelected(r)) continue;
                if (ci < 0 || cj < 0) continue;
                if (ds.isMissing(r, static_cast<size_t>(ci)) || ds.isMissing(r, static_cast<size_t>(cj))) continue;
                x.push_back(ds.getNumeric(r, static_cast<size_t>(ci)));
                y.push_back(ds.getNumeric(r, static_cast<size_t>(cj)));
                w.push_back(ds.caseWeight(r));
            }
            row.push_back(stats::cell(stats::pearson(x, y, w)));
        }
        t.cells.push_back(row);
    }
    auto ti = doc.addItem(t);
    doc.addToOutline("Tables", ti, t.title);
    return doc;
}

OutputDocument ProcedureRunner::regression(const DatasetStore& ds, const RegressionOptions& opt) {
    OutputDocument doc;
    int dy = ds.indexOf(opt.dependent);
    if (dy < 0 || opt.independents.empty()) return doc;
    size_t p = opt.independents.size();
    std::vector<int> ix(p);
    for (size_t i = 0; i < p; ++i) {
        ix[i] = ds.indexOf(opt.independents[i]);
        if (ix[i] < 0) return doc;
    }

    // Collect design matrix with intercept
    std::vector<std::vector<double>> X;
    std::vector<double> Y, W;
    for (size_t r = 0; r < ds.caseCount(); ++r) {
        if (!ds.caseSelected(r) || ds.isMissing(r, static_cast<size_t>(dy))) continue;
        bool ok = true;
        std::vector<double> row(p + 1, 1.0);
        for (size_t j = 0; j < p; ++j) {
            if (ds.isMissing(r, static_cast<size_t>(ix[j]))) { ok = false; break; }
            row[j + 1] = ds.getNumeric(r, static_cast<size_t>(ix[j]));
        }
        if (!ok) continue;
        X.push_back(row);
        Y.push_back(ds.getNumeric(r, static_cast<size_t>(dy)));
        W.push_back(ds.caseWeight(r));
    }
    size_t n = X.size();
    size_t k = p + 1;
    if (n < k) {
        auto w = doc.addItem(OutputWarning{"Insufficient cases for regression"});
        doc.addToOutline("Warnings", w, "REGRESSION");
        return doc;
    }

    // Normal equations XtWX beta = XtWY
    std::vector<std::vector<double>> A(k, std::vector<double>(k, 0));
    std::vector<double> b(k, 0);
    for (size_t i = 0; i < n; ++i) {
        for (size_t r = 0; r < k; ++r) {
            b[r] += W[i] * X[i][r] * Y[i];
            for (size_t c = 0; c < k; ++c) A[r][c] += W[i] * X[i][r] * X[i][c];
        }
    }
    // Gaussian elimination
    std::vector<std::vector<double>> M = A;
    std::vector<double> rhs = b;
    for (size_t col = 0; col < k; ++col) {
        size_t piv = col;
        for (size_t r = col + 1; r < k; ++r) if (std::fabs(M[r][col]) > std::fabs(M[piv][col])) piv = r;
        std::swap(M[col], M[piv]); std::swap(rhs[col], rhs[piv]);
        double div = M[col][col];
        if (std::fabs(div) < 1e-12) continue;
        for (size_t c = col; c < k; ++c) M[col][c] /= div;
        rhs[col] /= div;
        for (size_t r = 0; r < k; ++r) {
            if (r == col) continue;
            double f = M[r][col];
            for (size_t c = col; c < k; ++c) M[r][c] -= f * M[col][c];
            rhs[r] -= f * rhs[col];
        }
    }
    std::vector<double> beta = rhs;

    double yMean = 0, wSum = 0;
    for (size_t i = 0; i < n; ++i) { yMean += W[i]*Y[i]; wSum += W[i]; }
    yMean /= wSum;
    double sst = 0, sse = 0;
    for (size_t i = 0; i < n; ++i) {
        double yhat = 0;
        for (size_t j = 0; j < k; ++j) yhat += beta[j] * X[i][j];
        sse += W[i] * (Y[i] - yhat) * (Y[i] - yhat);
        sst += W[i] * (Y[i] - yMean) * (Y[i] - yMean);
    }
    double r2 = sst > 0 ? 1.0 - sse / sst : NAN;
    double adj = (wSum - k) > 0 ? 1.0 - (1.0 - r2) * (wSum - 1) / (wSum - k) : NAN;

    PivotTable model;
    model.title = "Model Summary";
    model.columnLabels = {"R", "R Square", "Adjusted R Square", "Std. Error of the Estimate"};
    model.rowLabels = {"1"};
    double seEst = (wSum - k) > 0 ? std::sqrt(sse / (wSum - k)) : NAN;
    model.cells.push_back({stats::cell(std::sqrt(std::max(0.0, r2))), stats::cell(r2), stats::cell(adj), stats::cell(seEst)});
    auto mi = doc.addItem(model);
    doc.addToOutline("Tables", mi, model.title);

    PivotTable coef;
    coef.title = "Coefficients";
    coef.columnLabels = {"B", "Std. Error", "t", "Sig."};
    coef.rowLabels.push_back("(Constant)");
    for (const auto& name : opt.independents) coef.rowLabels.push_back(name);
    // SE approx from (XtWX)^-1 * mse — use diagonal of A inverse approx via already-solved M as I after GE
    // Simplified: report B only with NaN SE if we didn't keep inverse; compute crude SE = seEst for intercept
    for (size_t j = 0; j < k; ++j) {
        coef.cells.push_back({stats::cell(beta[j]), stats::cell(NAN), stats::cell(NAN), stats::cell(NAN)});
    }
    auto ci = doc.addItem(coef);
    doc.addToOutline("Tables", ci, coef.title);

    auto note = doc.addItem(OutputNote{"Dependent Variable: " + opt.dependent});
    doc.addToOutline("Notes", note, "REGRESSION");
    return doc;
}

OutputDocument ProcedureRunner::npar(const DatasetStore& ds, const NParOptions& opt) {
    OutputDocument doc;
    if (opt.variables.empty()) return doc;
    int col = ds.indexOf(opt.variables[0]);
    if (col < 0) return doc;

    PivotTable t;
    t.title = "Nonparametric Tests";
    t.columnLabels = {"N", "Mean Rank / Statistic", "Asymp. Sig."};

    if (opt.test == NParOptions::Test::Wilcoxon || opt.test == NParOptions::Test::Sign) {
        // one-sample vs 0 using signs
        double plus = 0, minus = 0, n = 0;
        for (size_t r = 0; r < ds.caseCount(); ++r) {
            if (!ds.caseSelected(r) || ds.isMissing(r, static_cast<size_t>(col))) continue;
            double v = ds.getNumeric(r, static_cast<size_t>(col));
            double w = ds.caseWeight(r);
            if (v > 0) plus += w;
            else if (v < 0) minus += w;
            n += w;
        }
        double z = (plus + minus) > 0 ? (plus - minus) / std::sqrt(plus + minus) : NAN;
        double p = 2 * (1 - stats::normCdf(std::fabs(z)));
        t.rowLabels.push_back(opt.variables[0]);
        t.cells.push_back({stats::cell(n,0), stats::cell(z), stats::cell(p)});
    } else if (opt.test == NParOptions::Test::MannWhitney || opt.test == NParOptions::Test::KruskalWallis) {
        int gcol = ds.indexOf(opt.groupVariable);
        if (gcol < 0) return doc;
        // Kruskal-Wallis
        std::vector<std::pair<double,double>> vals; // value, group
        std::vector<double> weights;
        for (size_t r = 0; r < ds.caseCount(); ++r) {
            if (!ds.caseSelected(r)) continue;
            if (ds.isMissing(r, static_cast<size_t>(col)) || ds.isMissing(r, static_cast<size_t>(gcol))) continue;
            vals.push_back({ds.getNumeric(r, static_cast<size_t>(col)), ds.getNumeric(r, static_cast<size_t>(gcol))});
            weights.push_back(ds.caseWeight(r));
        }
        std::vector<size_t> order(vals.size());
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b){ return vals[a].first < vals[b].first; });
        std::vector<double> ranks(vals.size());
        for (size_t i = 0; i < order.size();) {
            size_t j = i;
            while (j < order.size() && vals[order[j]].first == vals[order[i]].first) ++j;
            double avg = (i + 1 + j) / 2.0;
            for (size_t k = i; k < j; ++k) ranks[order[k]] = avg;
            i = j;
        }
        std::map<double, double> rankSum, nG;
        double N = 0;
        for (size_t i = 0; i < vals.size(); ++i) {
            rankSum[vals[i].second] += ranks[i] * weights[i];
            nG[vals[i].second] += weights[i];
            N += weights[i];
        }
        double H = 0;
        for (auto& [g, rs] : rankSum) {
            H += rs * rs / nG[g];
        }
        H = 12.0 / (N * (N + 1)) * H - 3.0 * (N + 1);
        int df = static_cast<int>(nG.size()) - 1;
        double p = stats::chiSquareP(H, df);
        t.rowLabels.push_back(opt.variables[0]);
        t.cells.push_back({stats::cell(N,0), stats::cell(H), stats::cell(p)});
        t.footnotes.push_back("Kruskal-Wallis H, df=" + std::to_string(df));
    }
    auto ti = doc.addItem(t);
    doc.addToOutline("Tables", ti, t.title);
    return doc;
}

} // namespace spss
