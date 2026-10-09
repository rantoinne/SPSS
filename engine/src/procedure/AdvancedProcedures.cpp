#include "spss/Procedure.hpp"
#include "StatsUtil.hpp"
#include <map>
#include <set>
#include <cmath>
#include <numeric>
#include <limits>
#include <random>
#include <algorithm>

namespace spss {
namespace {

std::vector<size_t> completeCases(const DatasetStore& ds, const std::vector<int>& cols) {
    std::vector<size_t> rows;
    for (size_t r = 0; r < ds.caseCount(); ++r) {
        if (!ds.caseSelected(r)) continue;
        bool ok = true;
        for (int c : cols) {
            if (c < 0 || ds.isMissing(r, static_cast<size_t>(c))) { ok = false; break; }
        }
        if (ok) rows.push_back(r);
    }
    return rows;
}

bool solveLinear(std::vector<std::vector<double>> A, std::vector<double> b, std::vector<double>& x) {
    size_t n = A.size();
    if (n == 0 || b.size() != n) return false;
    for (size_t col = 0; col < n; ++col) {
        size_t piv = col;
        for (size_t r = col + 1; r < n; ++r)
            if (std::fabs(A[r][col]) > std::fabs(A[piv][col])) piv = r;
        std::swap(A[col], A[piv]);
        std::swap(b[col], b[piv]);
        double div = A[col][col];
        if (std::fabs(div) < 1e-14) return false;
        for (size_t c = col; c < n; ++c) A[col][c] /= div;
        b[col] /= div;
        for (size_t r = 0; r < n; ++r) {
            if (r == col) continue;
            double f = A[r][col];
            for (size_t c = col; c < n; ++c) A[r][c] -= f * A[col][c];
            b[r] -= f * b[col];
        }
    }
    x = b;
    return true;
}

// Power iteration for top eigenvector of symmetric PSD matrix (covariance).
bool powerEigen(const std::vector<std::vector<double>>& S, std::vector<double>& vec, double& val, int iters = 80) {
    size_t p = S.size();
    if (p == 0) return false;
    vec.assign(p, 1.0 / std::sqrt(static_cast<double>(p)));
    for (int it = 0; it < iters; ++it) {
        std::vector<double> y(p, 0);
        for (size_t i = 0; i < p; ++i)
            for (size_t j = 0; j < p; ++j) y[i] += S[i][j] * vec[j];
        double norm = 0;
        for (double v : y) norm += v * v;
        norm = std::sqrt(norm);
        if (norm < 1e-15) return false;
        for (size_t i = 0; i < p; ++i) vec[i] = y[i] / norm;
    }
    val = 0;
    for (size_t i = 0; i < p; ++i)
        for (size_t j = 0; j < p; ++j) val += vec[i] * S[i][j] * vec[j];
    return true;
}

void deflate(std::vector<std::vector<double>>& S, const std::vector<double>& v, double lambda) {
    size_t p = S.size();
    for (size_t i = 0; i < p; ++i)
        for (size_t j = 0; j < p; ++j)
            S[i][j] -= lambda * v[i] * v[j];
}

double logFact(int n) {
    double s = 0;
    for (int i = 2; i <= n; ++i) s += std::log(static_cast<double>(i));
    return s;
}

double hypergeomPMF(int a, int b, int c, int d) {
    // 2x2 table [[a,b],[c,d]]
    int n = a + b + c + d;
    int r1 = a + b, c1 = a + c;
    return std::exp(logFact(r1) + logFact(n - r1) + logFact(c1) + logFact(n - c1)
                    - logFact(a) - logFact(b) - logFact(c) - logFact(d) - logFact(n));
}

} // namespace

OutputDocument ProcedureRunner::glmUnivariate(const DatasetStore& ds, const GLMOptions& opt) {
    OutputDocument doc;
    auto logIdx = doc.addItem(OutputLogEntry{"UNIANOVA", "Running GLM Univariate (fixed-effects MVP)", false});
    doc.addToOutline("Log", logIdx, "UNIANOVA");

    int dy = ds.indexOf(opt.dependent);
    if (dy < 0) {
        auto w = doc.addItem(OutputWarning{"Dependent not found"});
        doc.addToOutline("Warnings", w, "UNIANOVA");
        return doc;
    }

    // Build design: intercept + factor dummies (drop first level) + covariates
    std::vector<int> factorCols, covCols;
    for (const auto& f : opt.factors) {
        int c = ds.indexOf(f);
        if (c >= 0) factorCols.push_back(c);
    }
    for (const auto& f : opt.covariates) {
        int c = ds.indexOf(f);
        if (c >= 0) covCols.push_back(c);
    }
    std::vector<int> allCols = {dy};
    allCols.insert(allCols.end(), factorCols.begin(), factorCols.end());
    allCols.insert(allCols.end(), covCols.begin(), covCols.end());
    auto rows = completeCases(ds, allCols);
    if (rows.empty()) {
        auto w = doc.addItem(OutputWarning{"No complete cases"});
        doc.addToOutline("Warnings", w, "UNIANOVA");
        return doc;
    }

    std::vector<std::map<double, int>> levelIndex(factorCols.size());
    for (size_t fi = 0; fi < factorCols.size(); ++fi) {
        std::set<double> levels;
        for (size_t r : rows) levels.insert(ds.getNumeric(r, static_cast<size_t>(factorCols[fi])));
        int idx = 0;
        for (double lv : levels) levelIndex[fi][lv] = idx++;
    }

    size_t p = 1; // intercept
    std::vector<std::string> colnames = {"(Intercept)"};
    for (size_t fi = 0; fi < factorCols.size(); ++fi) {
        int nLevels = static_cast<int>(levelIndex[fi].size());
        for (int k = 1; k < nLevels; ++k) {
            colnames.push_back(opt.factors[fi] + "=" + stats::fmt(k, 0));
            ++p;
        }
    }
    for (const auto& name : opt.covariates) {
        colnames.push_back(name);
        ++p;
    }

    std::vector<std::vector<double>> X(rows.size(), std::vector<double>(p, 0));
    std::vector<double> Y(rows.size()), W(rows.size());
    for (size_t i = 0; i < rows.size(); ++i) {
        size_t r = rows[i];
        X[i][0] = 1.0;
        size_t col = 1;
        for (size_t fi = 0; fi < factorCols.size(); ++fi) {
            int nLevels = static_cast<int>(levelIndex[fi].size());
            int lv = levelIndex[fi][ds.getNumeric(r, static_cast<size_t>(factorCols[fi]))];
            for (int k = 1; k < nLevels; ++k) {
                X[i][col++] = (lv == k) ? 1.0 : 0.0;
            }
        }
        for (int c : covCols) {
            X[i][col++] = ds.getNumeric(r, static_cast<size_t>(c));
        }
        Y[i] = ds.getNumeric(r, static_cast<size_t>(dy));
        W[i] = ds.caseWeight(r);
    }

    size_t n = rows.size();
    std::vector<std::vector<double>> A(p, std::vector<double>(p, 0));
    std::vector<double> b(p, 0);
    for (size_t i = 0; i < n; ++i) {
        for (size_t r = 0; r < p; ++r) {
            b[r] += W[i] * X[i][r] * Y[i];
            for (size_t c = 0; c < p; ++c) A[r][c] += W[i] * X[i][r] * X[i][c];
        }
    }
    std::vector<double> beta;
    if (!solveLinear(A, b, beta)) {
        auto w = doc.addItem(OutputWarning{"Singular design matrix"});
        doc.addToOutline("Warnings", w, "UNIANOVA");
        return doc;
    }

    double yMean = 0, wSum = 0, sse = 0, sst = 0;
    for (size_t i = 0; i < n; ++i) { yMean += W[i] * Y[i]; wSum += W[i]; }
    yMean /= wSum;
    for (size_t i = 0; i < n; ++i) {
        double yhat = 0;
        for (size_t j = 0; j < p; ++j) yhat += beta[j] * X[i][j];
        sse += W[i] * (Y[i] - yhat) * (Y[i] - yhat);
        sst += W[i] * (Y[i] - yMean) * (Y[i] - yMean);
    }
    double r2 = sst > 0 ? 1.0 - sse / sst : NAN;
    double mse = (wSum > p) ? sse / (wSum - p) : NAN;
    double msr = (p > 1 && wSum > p) ? ((sst - sse) / (p - 1)) : NAN;
    double F = (mse > 0 && !std::isnan(msr)) ? msr / mse : NAN;

    PivotTable tests;
    tests.title = "Tests of Between-Subjects Effects";
    tests.columnLabels = {"Type III Sum of Squares", "df", "Mean Square", "F", "Sig."};
    tests.rowLabels = {"Corrected Model", "Error", "Total"};
    double pModel = (!std::isnan(F)) ? stats::chiSquareP(F * (p - 1), static_cast<int>(p - 1)) : NAN; // rough
    tests.cells.push_back({stats::cell(sst - sse), stats::cell(static_cast<double>(p - 1), 0), stats::cell(msr), stats::cell(F), stats::cell(pModel)});
    tests.cells.push_back({stats::cell(sse), stats::cell(wSum - p, 0), stats::cell(mse), stats::cell(NAN), stats::cell(NAN)});
    tests.cells.push_back({stats::cell(sst), stats::cell(wSum - 1, 0), stats::cell(NAN), stats::cell(NAN), stats::cell(NAN)});
    tests.footnotes.push_back("R Squared = " + stats::fmt(r2) + " (fixed-effects OLS; not full SPSS UNIANOVA parity)");
    auto ti = doc.addItem(tests);
    doc.addToOutline("Tables", ti, tests.title);

    PivotTable coef;
    coef.title = "Parameter Estimates";
    coef.columnLabels = {"B"};
    for (size_t j = 0; j < p; ++j) {
        coef.rowLabels.push_back(colnames[j]);
        coef.cells.push_back({stats::cell(beta[j])});
    }
    auto ci = doc.addItem(coef);
    doc.addToOutline("Tables", ci, coef.title);
    return doc;
}

OutputDocument ProcedureRunner::factorPCA(const DatasetStore& ds, const FactorOptions& opt) {
    OutputDocument doc;
    auto logIdx = doc.addItem(OutputLogEntry{"FACTOR", "Running PCA (MVP)", false});
    doc.addToOutline("Log", logIdx, "FACTOR");

    std::vector<int> cols;
    for (const auto& name : opt.variables) {
        int c = ds.indexOf(name);
        if (c >= 0) cols.push_back(c);
    }
    if (cols.size() < 2) {
        auto w = doc.addItem(OutputWarning{"FACTOR needs at least 2 variables"});
        doc.addToOutline("Warnings", w, "FACTOR");
        return doc;
    }
    auto rows = completeCases(ds, cols);
    size_t n = rows.size(), p = cols.size();
    if (n < 2) {
        auto w = doc.addItem(OutputWarning{"Insufficient cases"});
        doc.addToOutline("Warnings", w, "FACTOR");
        return doc;
    }

    std::vector<double> mean(p, 0), wsum(p, 0);
    for (size_t r : rows) {
        double w = ds.caseWeight(r);
        for (size_t j = 0; j < p; ++j) {
            mean[j] += w * ds.getNumeric(r, static_cast<size_t>(cols[j]));
            wsum[j] += w;
        }
    }
    for (size_t j = 0; j < p; ++j) mean[j] /= wsum[j];

    std::vector<std::vector<double>> S(p, std::vector<double>(p, 0));
    double N = 0;
    for (size_t r : rows) {
        double w = ds.caseWeight(r);
        N += w;
        for (size_t i = 0; i < p; ++i) {
            double xi = ds.getNumeric(r, static_cast<size_t>(cols[i])) - mean[i];
            for (size_t j = 0; j < p; ++j) {
                double xj = ds.getNumeric(r, static_cast<size_t>(cols[j])) - mean[j];
                S[i][j] += w * xi * xj;
            }
        }
    }
    for (size_t i = 0; i < p; ++i)
        for (size_t j = 0; j < p; ++j) S[i][j] /= (N - 1.0);

    int nFac = opt.nFactors > 0 ? opt.nFactors : static_cast<int>(std::min(p, n - 1));
    nFac = std::max(1, std::min(nFac, static_cast<int>(p)));

    PivotTable ev;
    ev.title = "Total Variance Explained";
    ev.columnLabels = {"Component", "Eigenvalue", "% of Variance", "Cumulative %"};
    double trace = 0;
    for (size_t i = 0; i < p; ++i) trace += S[i][i];

    auto Scopy = S;
    double cum = 0;
    std::vector<std::vector<double>> loadings(p);
    std::vector<std::string> pcLabels;
    for (int f = 0; f < nFac; ++f) {
        std::vector<double> vec;
        double lambda = 0;
        if (!powerEigen(Scopy, vec, lambda)) break;
        deflate(Scopy, vec, lambda);
        double pct = trace > 0 ? 100.0 * lambda / trace : 0;
        cum += pct;
        ev.rowLabels.push_back(std::to_string(f + 1));
        ev.cells.push_back({stats::cell(static_cast<double>(f + 1), 0), stats::cell(lambda), stats::cell(pct, 2), stats::cell(cum, 2)});
        double scale = lambda > 0 ? std::sqrt(lambda) : 0;
        pcLabels.push_back("PC" + std::to_string(f + 1));
        for (size_t j = 0; j < p; ++j) loadings[j].push_back(vec[j] * scale);
    }

    PivotTable load;
    load.title = "Component Matrix";
    load.columnLabels = pcLabels;
    for (size_t j = 0; j < p; ++j) {
        load.rowLabels.push_back(ds.variableAt(static_cast<size_t>(cols[j])).name);
        std::vector<PivotCell> row;
        for (double v : loadings[j]) row.push_back(stats::cell(v));
        load.cells.push_back(row);
    }
    auto ei = doc.addItem(ev);
    doc.addToOutline("Tables", ei, ev.title);
    auto li = doc.addItem(load);
    doc.addToOutline("Tables", li, load.title);
    auto note = doc.addItem(OutputNote{"PCA via power iteration on covariance; not full FACTOR method/rotation parity."});
    doc.addToOutline("Notes", note, "FACTOR");
    return doc;
}

OutputDocument ProcedureRunner::quickCluster(const DatasetStore& ds, const ClusterOptions& opt) {
    OutputDocument doc;
    auto logIdx = doc.addItem(OutputLogEntry{"QUICK CLUSTER", "Running k-means (MVP)", false});
    doc.addToOutline("Log", logIdx, "QUICK CLUSTER");

    std::vector<int> cols;
    for (const auto& name : opt.variables) {
        int c = ds.indexOf(name);
        if (c >= 0) cols.push_back(c);
    }
    int k = std::max(2, opt.k);
    auto rows = completeCases(ds, cols);
    if (rows.size() < static_cast<size_t>(k) || cols.empty()) {
        auto w = doc.addItem(OutputWarning{"Insufficient data for clustering"});
        doc.addToOutline("Warnings", w, "CLUSTER");
        return doc;
    }
    size_t p = cols.size();
    std::vector<std::vector<double>> pts(rows.size(), std::vector<double>(p));
    for (size_t i = 0; i < rows.size(); ++i)
        for (size_t j = 0; j < p; ++j)
            pts[i][j] = ds.getNumeric(rows[i], static_cast<size_t>(cols[j]));

    std::vector<std::vector<double>> centers(k, std::vector<double>(p));
    for (int c = 0; c < k; ++c) centers[c] = pts[static_cast<size_t>(c) * rows.size() / k];
    std::vector<int> assign(rows.size(), 0);

    for (int it = 0; it < opt.maxIter; ++it) {
        bool changed = false;
        for (size_t i = 0; i < pts.size(); ++i) {
            int best = 0;
            double bestD = std::numeric_limits<double>::infinity();
            for (int c = 0; c < k; ++c) {
                double d = 0;
                for (size_t j = 0; j < p; ++j) {
                    double diff = pts[i][j] - centers[c][j];
                    d += diff * diff;
                }
                if (d < bestD) { bestD = d; best = c; }
            }
            if (assign[i] != best) { assign[i] = best; changed = true; }
        }
        std::vector<std::vector<double>> sum(k, std::vector<double>(p, 0));
        std::vector<int> cnt(k, 0);
        for (size_t i = 0; i < pts.size(); ++i) {
            int c = assign[i];
            cnt[c]++;
            for (size_t j = 0; j < p; ++j) sum[c][j] += pts[i][j];
        }
        for (int c = 0; c < k; ++c) {
            if (cnt[c] == 0) continue;
            for (size_t j = 0; j < p; ++j) centers[c][j] = sum[c][j] / cnt[c];
        }
        if (!changed) break;
    }

    PivotTable finalC;
    finalC.title = "Final Cluster Centers";
    for (const auto& name : opt.variables) finalC.columnLabels.push_back(name);
    for (int c = 0; c < k; ++c) {
        finalC.rowLabels.push_back("Cluster " + std::to_string(c + 1));
        std::vector<PivotCell> row;
        for (size_t j = 0; j < p; ++j) row.push_back(stats::cell(centers[c][j]));
        finalC.cells.push_back(row);
    }
    auto ci = doc.addItem(finalC);
    doc.addToOutline("Tables", ci, finalC.title);

    PivotTable sizes;
    sizes.title = "Number of Cases in each Cluster";
    sizes.columnLabels = {"N"};
    std::vector<int> cnt(k, 0);
    for (int a : assign) cnt[a]++;
    for (int c = 0; c < k; ++c) {
        sizes.rowLabels.push_back("Cluster " + std::to_string(c + 1));
        sizes.cells.push_back({stats::cell(cnt[c], 0)});
    }
    auto si = doc.addItem(sizes);
    doc.addToOutline("Tables", si, sizes.title);
    return doc;
}

OutputDocument ProcedureRunner::logistic(const DatasetStore& ds, const LogisticOptions& opt) {
    OutputDocument doc;
    auto logIdx = doc.addItem(OutputLogEntry{"LOGISTIC REGRESSION", "Running binary logistic (IRLS MVP)", false});
    doc.addToOutline("Log", logIdx, "LOGISTIC REGRESSION");

    int dy = ds.indexOf(opt.dependent);
    std::vector<int> ix;
    for (const auto& name : opt.independents) {
        int c = ds.indexOf(name);
        if (c >= 0) ix.push_back(c);
    }
    if (dy < 0 || ix.empty()) {
        auto w = doc.addItem(OutputWarning{"LOGISTIC needs dependent and predictors"});
        doc.addToOutline("Warnings", w, "LOGISTIC");
        return doc;
    }
    std::vector<int> all = {dy};
    all.insert(all.end(), ix.begin(), ix.end());
    auto rows = completeCases(ds, all);
    size_t n = rows.size(), p = ix.size() + 1;
    if (n < p + 1) {
        auto w = doc.addItem(OutputWarning{"Insufficient cases"});
        doc.addToOutline("Warnings", w, "LOGISTIC");
        return doc;
    }

    std::vector<std::vector<double>> X(n, std::vector<double>(p, 1.0));
    std::vector<double> Y(n);
    for (size_t i = 0; i < n; ++i) {
        size_t r = rows[i];
        double y = ds.getNumeric(r, static_cast<size_t>(dy));
        Y[i] = (y > 0.5) ? 1.0 : 0.0;
        for (size_t j = 0; j < ix.size(); ++j)
            X[i][j + 1] = ds.getNumeric(r, static_cast<size_t>(ix[j]));
    }

    std::vector<double> beta(p, 0);
    bool ok = true;
    for (int it = 0; it < opt.maxIter; ++it) {
        std::vector<std::vector<double>> H(p, std::vector<double>(p, 0));
        std::vector<double> g(p, 0);
        for (size_t i = 0; i < n; ++i) {
            double eta = 0;
            for (size_t j = 0; j < p; ++j) eta += beta[j] * X[i][j];
            double mu = 1.0 / (1.0 + std::exp(-std::clamp(eta, -30.0, 30.0)));
            double w = mu * (1.0 - mu);
            if (w < 1e-12) w = 1e-12;
            double resid = Y[i] - mu;
            for (size_t r = 0; r < p; ++r) {
                g[r] += X[i][r] * resid;
                for (size_t c = 0; c < p; ++c) H[r][c] += w * X[i][r] * X[i][c];
            }
        }
        std::vector<double> step;
        if (!solveLinear(H, g, step)) { ok = false; break; }
        double maxStep = 0;
        for (size_t j = 0; j < p; ++j) {
            beta[j] += step[j];
            maxStep = std::max(maxStep, std::fabs(step[j]));
        }
        if (maxStep < 1e-6) break;
    }

    if (!ok) {
        auto w = doc.addItem(OutputWarning{"IRLS failed to converge (singular Hessian)"});
        doc.addToOutline("Warnings", w, "LOGISTIC");
    }

    double ll = 0, ll0 = 0;
    double ybar = 0;
    for (double y : Y) ybar += y;
    ybar /= n;
    for (size_t i = 0; i < n; ++i) {
        double eta = 0;
        for (size_t j = 0; j < p; ++j) eta += beta[j] * X[i][j];
        double mu = 1.0 / (1.0 + std::exp(-std::clamp(eta, -30.0, 30.0)));
        mu = std::clamp(mu, 1e-12, 1 - 1e-12);
        ll += Y[i] * std::log(mu) + (1 - Y[i]) * std::log(1 - mu);
        double m0 = std::clamp(ybar, 1e-12, 1 - 1e-12);
        ll0 += Y[i] * std::log(m0) + (1 - Y[i]) * std::log(1 - m0);
    }
    double r2 = 1.0 - ll / ll0;

    PivotTable model;
    model.title = "Model Summary";
    model.columnLabels = {"-2 Log likelihood", "Cox & Snell R Square (approx)", "N"};
    model.rowLabels = {"1"};
    model.cells.push_back({stats::cell(-2 * ll), stats::cell(r2), stats::cell(static_cast<double>(n), 0)});
    auto mi = doc.addItem(model);
    doc.addToOutline("Tables", mi, model.title);

    PivotTable coef;
    coef.title = "Variables in the Equation";
    coef.columnLabels = {"B", "Exp(B)"};
    coef.rowLabels.push_back("(Constant)");
    for (const auto& name : opt.independents) coef.rowLabels.push_back(name);
    for (size_t j = 0; j < p; ++j)
        coef.cells.push_back({stats::cell(beta[j]), stats::cell(std::exp(beta[j]))});
    auto ci = doc.addItem(coef);
    doc.addToOutline("Tables", ci, coef.title);
    return doc;
}

OutputDocument ProcedureRunner::kaplanMeier(const DatasetStore& ds, const SurvivalKMOptions& opt) {
    OutputDocument doc;
    auto logIdx = doc.addItem(OutputLogEntry{"KM", "Running Kaplan-Meier (MVP)", false});
    doc.addToOutline("Log", logIdx, "KM");

    int tc = ds.indexOf(opt.time);
    int sc = ds.indexOf(opt.status);
    if (tc < 0 || sc < 0) {
        auto w = doc.addItem(OutputWarning{"KM needs TIME and STATUS"});
        doc.addToOutline("Warnings", w, "KM");
        return doc;
    }
    struct Event { double t; int event; double w; };
    std::vector<Event> ev;
    for (size_t r = 0; r < ds.caseCount(); ++r) {
        if (!ds.caseSelected(r) || ds.isMissing(r, static_cast<size_t>(tc)) || ds.isMissing(r, static_cast<size_t>(sc))) continue;
        ev.push_back({ds.getNumeric(r, static_cast<size_t>(tc)),
                      ds.getNumeric(r, static_cast<size_t>(sc)) > 0.5 ? 1 : 0,
                      ds.caseWeight(r)});
    }
    std::sort(ev.begin(), ev.end(), [](const Event& a, const Event& b) { return a.t < b.t; });
    double atRisk = 0;
    for (const auto& e : ev) atRisk += e.w;

    PivotTable t;
    t.title = "Survival Table";
    t.columnLabels = {"Time", "Status", "Cumulative Survival", "At Risk"};
    double S = 1.0;
    size_t i = 0;
    while (i < ev.size()) {
        double tcur = ev[i].t;
        double deaths = 0, cens = 0;
        while (i < ev.size() && ev[i].t == tcur) {
            if (ev[i].event) deaths += ev[i].w;
            else cens += ev[i].w;
            ++i;
        }
        if (deaths > 0 && atRisk > 0) S *= (1.0 - deaths / atRisk);
        t.rowLabels.push_back(stats::fmt(tcur, 2));
        t.cells.push_back({stats::cell(tcur, 2), stats::cell(deaths, 0), stats::cell(S), stats::cell(atRisk, 1)});
        atRisk -= deaths + cens;
    }
    auto ti = doc.addItem(t);
    doc.addToOutline("Tables", ti, t.title);
    ChartSpec ch;
    ch.title = "Survival Function";
    ch.type = "line";
    ch.xLabel = "Time";
    ch.yLabel = "S(t)";
    double Sp = 1.0;
    double risk = 0;
    for (const auto& e : ev) risk += e.w;
    size_t j = 0;
    while (j < ev.size()) {
        double tcur = ev[j].t;
        double deaths = 0, cens = 0;
        while (j < ev.size() && ev[j].t == tcur) {
            if (ev[j].event) deaths += ev[j].w;
            else cens += ev[j].w;
            ++j;
        }
        if (deaths > 0 && risk > 0) Sp *= (1.0 - deaths / risk);
        ch.categories.push_back(stats::fmt(tcur, 2));
        ch.values.push_back(Sp);
        risk -= deaths + cens;
    }
    auto ci = doc.addItem(ch);
    doc.addToOutline("Charts", ci, ch.title);
    return doc;
}

OutputDocument ProcedureRunner::multipleImputation(const DatasetStore& ds, const ImputationOptions& opt) {
    OutputDocument doc;
    auto logIdx = doc.addItem(OutputLogEntry{"MULTIPLE IMPUTATION", "Mean imputation diagnostics (MVP)", false});
    doc.addToOutline("Log", logIdx, "MULTIPLE IMPUTATION");

    PivotTable t;
    t.title = "Univariate Statistics";
    t.columnLabels = {"N", "Mean", "Std. Deviation", "Missing", "Imputed Mean (M=" + std::to_string(opt.m) + ")"};
    for (const auto& name : opt.variables) {
        int col = ds.indexOf(name);
        if (col < 0) continue;
        auto s = stats::WeightedSample::fromVar(ds, static_cast<size_t>(col));
        double missing = 0;
        for (size_t r = 0; r < ds.caseCount(); ++r) {
            if (!ds.caseSelected(r)) continue;
            if (ds.isMissing(r, static_cast<size_t>(col))) missing += ds.caseWeight(r);
        }
        t.rowLabels.push_back(name);
        t.cells.push_back({stats::cell(s.n, 0), stats::cell(s.mean), stats::cell(s.sd),
                           stats::cell(missing, 0), stats::cell(s.mean)});
    }
    t.footnotes.push_back("MVP fills missing with observed mean across M implicates; not full FCS/MI pooling.");
    auto ti = doc.addItem(t);
    doc.addToOutline("Tables", ti, t.title);
    return doc;
}

OutputDocument ProcedureRunner::bootstrapMeans(const DatasetStore& ds, const BootstrapOptions& opt) {
    OutputDocument doc;
    auto logIdx = doc.addItem(OutputLogEntry{"BOOTSTRAP", "Bootstrap means (MVP)", false});
    doc.addToOutline("Log", logIdx, "BOOTSTRAP");

    std::mt19937 rng(opt.seed);
    PivotTable t;
    t.title = "Bootstrap Statistics";
    t.columnLabels = {"Statistic", "Bias", "Std. Error", "95% Lower", "95% Upper"};

    for (const auto& name : opt.variables) {
        int col = ds.indexOf(name);
        if (col < 0) continue;
        std::vector<double> vals;
        for (size_t r = 0; r < ds.caseCount(); ++r) {
            if (!ds.caseSelected(r) || ds.isMissing(r, static_cast<size_t>(col))) continue;
            vals.push_back(ds.getNumeric(r, static_cast<size_t>(col)));
        }
        if (vals.size() < 2) continue;
        double obs = std::accumulate(vals.begin(), vals.end(), 0.0) / vals.size();
        std::vector<double> boots;
        boots.reserve(opt.samples);
        std::uniform_int_distribution<size_t> dist(0, vals.size() - 1);
        for (int b = 0; b < opt.samples; ++b) {
            double sum = 0;
            for (size_t i = 0; i < vals.size(); ++i) sum += vals[dist(rng)];
            boots.push_back(sum / vals.size());
        }
        std::sort(boots.begin(), boots.end());
        double meanB = std::accumulate(boots.begin(), boots.end(), 0.0) / boots.size();
        double var = 0;
        for (double v : boots) var += (v - meanB) * (v - meanB);
        var /= (boots.size() - 1.0);
        size_t lo = static_cast<size_t>(0.025 * (boots.size() - 1));
        size_t hi = static_cast<size_t>(0.975 * (boots.size() - 1));
        t.rowLabels.push_back(name + " Mean");
        t.cells.push_back({stats::cell(obs), stats::cell(meanB - obs), stats::cell(std::sqrt(var)),
                           stats::cell(boots[lo]), stats::cell(boots[hi])});
    }
    auto ti = doc.addItem(t);
    doc.addToOutline("Tables", ti, t.title);
    return doc;
}

OutputDocument ProcedureRunner::forecastSMA(const DatasetStore& ds, const ForecastOptions& opt) {
    OutputDocument doc;
    auto logIdx = doc.addItem(OutputLogEntry{"TSMODEL", "Simple moving-average forecast (MVP)", false});
    doc.addToOutline("Log", logIdx, "TSMODEL");

    int col = ds.indexOf(opt.variable);
    if (col < 0) {
        auto w = doc.addItem(OutputWarning{"Series variable not found"});
        doc.addToOutline("Warnings", w, "TSMODEL");
        return doc;
    }
    std::vector<double> series;
    for (size_t r = 0; r < ds.caseCount(); ++r) {
        if (!ds.caseSelected(r) || ds.isMissing(r, static_cast<size_t>(col))) continue;
        series.push_back(ds.getNumeric(r, static_cast<size_t>(col)));
    }
    int w = std::max(1, opt.window);
    if (static_cast<int>(series.size()) < w) {
        auto warn = doc.addItem(OutputWarning{"Series shorter than SMA window"});
        doc.addToOutline("Warnings", warn, "TSMODEL");
        return doc;
    }
    double sma = 0;
    for (int i = 0; i < w; ++i) sma += series[series.size() - 1 - i];
    sma /= w;

    PivotTable t;
    t.title = "Forecast";
    t.columnLabels = {"Period", "Forecast"};
    for (int h = 1; h <= opt.periods; ++h) {
        t.rowLabels.push_back(std::to_string(h));
        t.cells.push_back({stats::cell(static_cast<double>(h), 0), stats::cell(sma)});
    }
    t.footnotes.push_back("Naive SMA(" + std::to_string(w) + "); not Expert Modeler / ARIMA parity.");
    auto ti = doc.addItem(t);
    doc.addToOutline("Tables", ti, t.title);
    return doc;
}

OutputDocument ProcedureRunner::ctables(const DatasetStore& ds, const CTablesOptions& opt) {
    OutputDocument doc;
    auto logIdx = doc.addItem(OutputLogEntry{"CTABLES", "Custom Tables MVP", false});
    doc.addToOutline("Log", logIdx, "CTABLES");

    int rcol = ds.indexOf(opt.row);
    int ccol = ds.indexOf(opt.column);
    if (rcol < 0 || ccol < 0) {
        auto w = doc.addItem(OutputWarning{"CTABLES needs row and column variables"});
        doc.addToOutline("Warnings", w, "CTABLES");
        return doc;
    }
    int scol = opt.summaryVar.empty() ? -1 : ds.indexOf(opt.summaryVar);

    std::set<double> rows, cols;
    for (size_t r = 0; r < ds.caseCount(); ++r) {
        if (!ds.caseSelected(r)) continue;
        if (ds.isMissing(r, static_cast<size_t>(rcol)) || ds.isMissing(r, static_cast<size_t>(ccol))) continue;
        rows.insert(ds.getNumeric(r, static_cast<size_t>(rcol)));
        cols.insert(ds.getNumeric(r, static_cast<size_t>(ccol)));
    }
    std::map<std::pair<double, double>, double> count, sum;
    for (size_t r = 0; r < ds.caseCount(); ++r) {
        if (!ds.caseSelected(r)) continue;
        if (ds.isMissing(r, static_cast<size_t>(rcol)) || ds.isMissing(r, static_cast<size_t>(ccol))) continue;
        auto key = std::make_pair(ds.getNumeric(r, static_cast<size_t>(rcol)),
                                  ds.getNumeric(r, static_cast<size_t>(ccol)));
        double w = ds.caseWeight(r);
        count[key] += w;
        if (scol >= 0 && !ds.isMissing(r, static_cast<size_t>(scol)))
            sum[key] += w * ds.getNumeric(r, static_cast<size_t>(scol));
    }

    PivotTable t;
    t.title = "Custom Table: " + opt.row + " × " + opt.column;
    for (double c : cols) t.columnLabels.push_back(stats::fmt(c, 2));
    t.columnLabels.push_back("Total");
    for (double rv : rows) {
        t.rowLabels.push_back(stats::fmt(rv, 2));
        std::vector<PivotCell> row;
        double rowTot = 0;
        for (double cv : cols) {
            auto key = std::make_pair(rv, cv);
            double v = count[key];
            if (scol >= 0 && v > 0) v = sum[key] / count[key];
            rowTot += count[key];
            row.push_back(stats::cell(v, scol >= 0 ? 4 : 1));
        }
        row.push_back(stats::cell(rowTot, 1));
        t.cells.push_back(row);
    }
    if (scol >= 0) t.footnotes.push_back("Cell values: mean of " + opt.summaryVar);
    else t.footnotes.push_back("Cell values: counts");
    auto ti = doc.addItem(t);
    doc.addToOutline("Tables", ti, t.title);
    return doc;
}

OutputDocument ProcedureRunner::decisionTree(const DatasetStore& ds, const TreeOptions& opt) {
    OutputDocument doc;
    auto logIdx = doc.addItem(OutputLogEntry{"TREE", "Single-split CART-style tree (MVP)", false});
    doc.addToOutline("Log", logIdx, "TREE");

    int dy = ds.indexOf(opt.dependent);
    int px = ds.indexOf(opt.predictor);
    if (dy < 0 || px < 0) {
        auto w = doc.addItem(OutputWarning{"TREE needs dependent and predictor"});
        doc.addToOutline("Warnings", w, "TREE");
        return doc;
    }
    std::vector<std::pair<double, double>> pts;
    for (size_t r = 0; r < ds.caseCount(); ++r) {
        if (!ds.caseSelected(r) || ds.isMissing(r, static_cast<size_t>(dy)) || ds.isMissing(r, static_cast<size_t>(px))) continue;
        pts.push_back({ds.getNumeric(r, static_cast<size_t>(px)), ds.getNumeric(r, static_cast<size_t>(dy))});
    }
    if (pts.size() < static_cast<size_t>(opt.minLeaf * 2)) {
        auto w = doc.addItem(OutputWarning{"Too few cases for split"});
        doc.addToOutline("Warnings", w, "TREE");
        return doc;
    }
    std::sort(pts.begin(), pts.end());
    auto mse = [](const std::vector<std::pair<double, double>>& v, size_t lo, size_t hi) {
        if (hi <= lo) return 0.0;
        double m = 0;
        for (size_t i = lo; i < hi; ++i) m += v[i].second;
        m /= (hi - lo);
        double s = 0;
        for (size_t i = lo; i < hi; ++i) {
            double d = v[i].second - m;
            s += d * d;
        }
        return s;
    };
    double bestGain = -1;
    size_t bestSplit = opt.minLeaf;
    double parent = mse(pts, 0, pts.size());
    for (size_t s = opt.minLeaf; s + opt.minLeaf <= pts.size(); ++s) {
        if (pts[s].first == pts[s - 1].first) continue;
        double gain = parent - mse(pts, 0, s) - mse(pts, s, pts.size());
        if (gain > bestGain) { bestGain = gain; bestSplit = s; }
    }
    if (bestSplit == 0 || bestSplit >= pts.size()) bestSplit = pts.size() / 2;
    double thr = (pts[bestSplit - 1].first + pts[bestSplit].first) / 2.0;
    auto meanRange = [&](size_t lo, size_t hi) {
        double m = 0;
        for (size_t i = lo; i < hi; ++i) m += pts[i].second;
        return m / (hi - lo);
    };

    PivotTable t;
    t.title = "Tree Table";
    t.columnLabels = {"Node", "N", "Mean Predicted", "Rule"};
    t.rowLabels = {"0", "1", "2"};
    auto ruleCell = [](const std::string& s) {
        PivotCell c;
        c.value = std::numeric_limits<double>::quiet_NaN();
        c.isMissing = true;
        c.display = s;
        return c;
    };
    t.cells.push_back({stats::cell(0, 0), stats::cell(static_cast<double>(pts.size()), 0),
                       stats::cell(meanRange(0, pts.size())), ruleCell("root")});
    t.cells.push_back({stats::cell(1, 0), stats::cell(static_cast<double>(bestSplit), 0),
                       stats::cell(meanRange(0, bestSplit)), ruleCell(opt.predictor + " <= " + stats::fmt(thr))});
    t.cells.push_back({stats::cell(2, 0), stats::cell(static_cast<double>(pts.size() - bestSplit), 0),
                       stats::cell(meanRange(bestSplit, pts.size())),
                       ruleCell(opt.predictor + " > " + stats::fmt(thr))});
    t.footnotes.push_back("Variance-reduction split; not full CHAID/CRT/QUEST parity.");
    auto ti = doc.addItem(t);
    doc.addToOutline("Tables", ti, t.title);
    return doc;
}

OutputDocument ProcedureRunner::fisherExact(const DatasetStore& ds, const ExactOptions& opt) {
    OutputDocument doc;
    auto logIdx = doc.addItem(OutputLogEntry{"EXACT", "Fisher exact 2×2 (MVP)", false});
    doc.addToOutline("Log", logIdx, "EXACT");

    CrosstabsOptions copt;
    copt.rows = {opt.row};
    copt.cols = {opt.column};
    copt.chiSquare = true;
    doc.append(crosstabs(ds, copt));

    int rcol = ds.indexOf(opt.row);
    int ccol = ds.indexOf(opt.column);
    if (rcol < 0 || ccol < 0) return doc;

    std::map<double, int> rMap, cMap;
    for (size_t r = 0; r < ds.caseCount(); ++r) {
        if (!ds.caseSelected(r) || ds.isMissing(r, static_cast<size_t>(rcol)) || ds.isMissing(r, static_cast<size_t>(ccol))) continue;
        double rv = ds.getNumeric(r, static_cast<size_t>(rcol));
        double cv = ds.getNumeric(r, static_cast<size_t>(ccol));
        if (!rMap.count(rv) && rMap.size() < 2) rMap[rv] = static_cast<int>(rMap.size());
        if (!cMap.count(cv) && cMap.size() < 2) cMap[cv] = static_cast<int>(cMap.size());
    }
    if (rMap.size() != 2 || cMap.size() != 2) {
        auto w = doc.addItem(OutputWarning{"Fisher exact MVP requires dichotomous row and column"});
        doc.addToOutline("Warnings", w, "EXACT");
        return doc;
    }
    int a = 0, b = 0, c = 0, d = 0;
    for (size_t r = 0; r < ds.caseCount(); ++r) {
        if (!ds.caseSelected(r) || ds.isMissing(r, static_cast<size_t>(rcol)) || ds.isMissing(r, static_cast<size_t>(ccol))) continue;
        int ri = rMap[ds.getNumeric(r, static_cast<size_t>(rcol))];
        int ci = cMap[ds.getNumeric(r, static_cast<size_t>(ccol))];
        int w = static_cast<int>(std::lround(ds.caseWeight(r)));
        if (ri == 0 && ci == 0) a += w;
        else if (ri == 0 && ci == 1) b += w;
        else if (ri == 1 && ci == 0) c += w;
        else d += w;
    }
    double pObs = hypergeomPMF(a, b, c, d);
    int r1 = a + b, c1 = a + c, n = a + b + c + d;
    int amin = std::max(0, c1 - (n - r1));
    int amax = std::min(r1, c1);
    double p = 0;
    for (int aa = amin; aa <= amax; ++aa) {
        int bb = r1 - aa, cc = c1 - aa, dd = n - r1 - c1 + aa;
        double pm = hypergeomPMF(aa, bb, cc, dd);
        if (pm <= pObs + 1e-12) p += pm;
    }
    PivotTable t;
    t.title = "Fisher's Exact Test";
    t.columnLabels = {"Exact Sig. (2-sided)"};
    t.rowLabels = {"Fisher's Exact Test"};
    t.cells.push_back({stats::cell(std::clamp(p, 0.0, 1.0))});
    auto ti = doc.addItem(t);
    doc.addToOutline("Tables", ti, t.title);
    return doc;
}

OutputDocument ProcedureRunner::neuralNet(const DatasetStore& ds, const NeuralNetOptions& opt) {
    OutputDocument doc;
    auto logIdx = doc.addItem(OutputLogEntry{"NEURAL NETWORK", "Single-layer perceptron (MVP)", false});
    doc.addToOutline("Log", logIdx, "NEURAL NETWORK");

    int dy = ds.indexOf(opt.dependent);
    std::vector<int> ix;
    for (const auto& name : opt.independents) {
        int c = ds.indexOf(name);
        if (c >= 0) ix.push_back(c);
    }
    if (dy < 0 || ix.empty()) {
        auto w = doc.addItem(OutputWarning{"NEURAL NETWORK needs dependent and predictors"});
        doc.addToOutline("Warnings", w, "NN");
        return doc;
    }
    std::vector<int> all = {dy};
    all.insert(all.end(), ix.begin(), ix.end());
    auto rows = completeCases(ds, all);
    size_t n = rows.size(), p = ix.size();
    if (n < 4) {
        auto w = doc.addItem(OutputWarning{"Insufficient cases"});
        doc.addToOutline("Warnings", w, "NN");
        return doc;
    }

    std::vector<std::vector<double>> X(n, std::vector<double>(p));
    std::vector<double> Y(n);
    std::vector<double> mean(p, 0), sd(p, 1);
    for (size_t i = 0; i < n; ++i) {
        Y[i] = ds.getNumeric(rows[i], static_cast<size_t>(dy)) > 0.5 ? 1.0 : 0.0;
        for (size_t j = 0; j < p; ++j) {
            X[i][j] = ds.getNumeric(rows[i], static_cast<size_t>(ix[j]));
            mean[j] += X[i][j];
        }
    }
    for (size_t j = 0; j < p; ++j) mean[j] /= n;
    for (size_t j = 0; j < p; ++j) {
        double v = 0;
        for (size_t i = 0; i < n; ++i) {
            double d = X[i][j] - mean[j];
            v += d * d;
        }
        sd[j] = std::sqrt(v / (n - 1));
        if (sd[j] < 1e-12) sd[j] = 1;
        for (size_t i = 0; i < n; ++i) X[i][j] = (X[i][j] - mean[j]) / sd[j];
    }

    std::vector<double> w(p, 0);
    double b = 0;
    for (int ep = 0; ep < opt.epochs; ++ep) {
        for (size_t i = 0; i < n; ++i) {
            double z = b;
            for (size_t j = 0; j < p; ++j) z += w[j] * X[i][j];
            double pred = 1.0 / (1.0 + std::exp(-std::clamp(z, -30.0, 30.0)));
            double err = Y[i] - pred;
            b += opt.lr * err;
            for (size_t j = 0; j < p; ++j) w[j] += opt.lr * err * X[i][j];
        }
    }
    int correct = 0;
    for (size_t i = 0; i < n; ++i) {
        double z = b;
        for (size_t j = 0; j < p; ++j) z += w[j] * X[i][j];
        double pred = 1.0 / (1.0 + std::exp(-z));
        if ((pred >= 0.5) == (Y[i] >= 0.5)) ++correct;
    }

    PivotTable t;
    t.title = "Network Performance";
    t.columnLabels = {"N", "Accuracy", "Hidden Layers"};
    t.rowLabels = {"Training"};
    t.cells.push_back({stats::cell(static_cast<double>(n), 0),
                       stats::cell(100.0 * correct / n, 2),
                       stats::cell(0, 0)});
    t.footnotes.push_back("Logistic perceptron only; not multilayer MLP / SPSS Neural Networks parity.");
    auto ti = doc.addItem(t);
    doc.addToOutline("Tables", ti, t.title);

    PivotTable syn;
    syn.title = "Synaptic Weights";
    syn.columnLabels = {"Weight"};
    syn.rowLabels.push_back("Bias");
    syn.cells.push_back({stats::cell(b)});
    for (size_t j = 0; j < p; ++j) {
        syn.rowLabels.push_back(opt.independents[j]);
        syn.cells.push_back({stats::cell(w[j])});
    }
    auto si = doc.addItem(syn);
    doc.addToOutline("Tables", si, syn.title);
    return doc;
}

} // namespace spss
