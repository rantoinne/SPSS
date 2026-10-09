#include "spss/capi.h"
#include "spss/DatasetStore.hpp"
#include "spss/Transform.hpp"
#include "spss/Procedure.hpp"
#include "spss/IO.hpp"
#include <string>
#include <mutex>
#include <cstring>

using namespace spss;

struct SPSS_Dataset {
    std::unique_ptr<DatasetStore> store;
};

struct SPSS_Output {
    OutputDocument doc;
    std::string htmlCache;
};

static std::mutex gErrMu;
static std::string gLastError;
static thread_local std::string gTlsString;

static void setError(const std::string& e) {
    std::lock_guard<std::mutex> lock(gErrMu);
    gLastError = e;
}

const char* spss_last_error(void) {
    std::lock_guard<std::mutex> lock(gErrMu);
    return gLastError.c_str();
}

SPSS_Dataset* spss_dataset_create(const char* name) {
    auto* d = new SPSS_Dataset;
    d->store = std::make_unique<DatasetStore>(name ? name : "Dataset1");
    return d;
}

void spss_dataset_destroy(SPSS_Dataset* ds) { delete ds; }

SPSS_Dataset* spss_dataset_clone(const SPSS_Dataset* ds) {
    if (!ds) return nullptr;
    auto* d = new SPSS_Dataset;
    d->store = ds->store->clone();
    return d;
}

const char* spss_dataset_name(const SPSS_Dataset* ds) {
    return ds ? ds->store->name().c_str() : "";
}

size_t spss_dataset_case_count(const SPSS_Dataset* ds) {
    return ds ? ds->store->caseCount() : 0;
}

size_t spss_dataset_var_count(const SPSS_Dataset* ds) {
    return ds ? ds->store->variableCount() : 0;
}

int spss_dataset_add_numeric(SPSS_Dataset* ds, const char* name, int width, int decimals) {
    try {
        Variable v;
        v.name = name;
        v.type = VarType::Numeric;
        v.width = width;
        v.decimals = decimals;
        return static_cast<int>(ds->store->addVariable(v));
    } catch (const std::exception& e) { setError(e.what()); return -1; }
}

int spss_dataset_add_string(SPSS_Dataset* ds, const char* name, int width) {
    try {
        Variable v;
        v.name = name;
        v.type = VarType::String;
        v.width = width;
        v.align = Align::Left;
        v.measure = Measure::Nominal;
        return static_cast<int>(ds->store->addVariable(v));
    } catch (const std::exception& e) { setError(e.what()); return -1; }
}

int spss_dataset_add_cases(SPSS_Dataset* ds, size_t n) {
    try { return static_cast<int>(ds->store->addCases(n)); }
    catch (const std::exception& e) { setError(e.what()); return -1; }
}

double spss_get_numeric(const SPSS_Dataset* ds, size_t row, size_t col) {
    return ds->store->getNumeric(row, col);
}

void spss_set_numeric(SPSS_Dataset* ds, size_t row, size_t col, double value) {
    ds->store->setNumeric(row, col, value);
}

const char* spss_get_string(const SPSS_Dataset* ds, size_t row, size_t col) {
    gTlsString = ds->store->getString(row, col);
    return gTlsString.c_str();
}

void spss_set_string(SPSS_Dataset* ds, size_t row, size_t col, const char* value) {
    ds->store->setString(row, col, value ? value : "");
}

const char* spss_var_name(const SPSS_Dataset* ds, size_t col) {
    return ds->store->variableAt(col).name.c_str();
}

const char* spss_var_label(const SPSS_Dataset* ds, size_t col) {
    return ds->store->variableAt(col).label.c_str();
}

void spss_set_var_label(SPSS_Dataset* ds, size_t col, const char* label) {
    ds->store->variableAt(col).label = label ? label : "";
}

int spss_var_type(const SPSS_Dataset* ds, size_t col) {
    return static_cast<int>(ds->store->variableAt(col).type);
}

int spss_var_measure(const SPSS_Dataset* ds, size_t col) {
    return static_cast<int>(ds->store->variableAt(col).measure);
}

void spss_set_var_measure(SPSS_Dataset* ds, size_t col, int measure) {
    ds->store->variableAt(col).measure = static_cast<Measure>(measure);
}

void spss_push_undo(SPSS_Dataset* ds) { ds->store->pushUndo(); }
int spss_undo(SPSS_Dataset* ds) {
    if (!ds->store->canUndo()) return 0;
    ds->store->undo();
    return 1;
}
int spss_redo(SPSS_Dataset* ds) {
    if (!ds->store->canRedo()) return 0;
    ds->store->redo();
    return 1;
}

int spss_read_csv(SPSS_Dataset** out, const char* path, int header) {
    try {
        ImportOptions opt;
        opt.firstRowHeader = header != 0;
        auto store = DataIO::readCSV(path, opt);
        auto* d = new SPSS_Dataset;
        d->store = std::move(store);
        *out = d;
        return 0;
    } catch (const std::exception& e) { setError(e.what()); return -1; }
}

int spss_write_csv(const SPSS_Dataset* ds, const char* path) {
    try { DataIO::writeCSV(*ds->store, path); return 0; }
    catch (const std::exception& e) { setError(e.what()); return -1; }
}

int spss_read_sav(SPSS_Dataset** out, const char* path) {
    try {
        auto store = DataIO::readSAV(path);
        auto* d = new SPSS_Dataset;
        d->store = std::move(store);
        *out = d;
        return 0;
    } catch (const std::exception& e) { setError(e.what()); return -1; }
}

int spss_write_sav(const SPSS_Dataset* ds, const char* path) {
    try { DataIO::writeSAV(*ds->store, path); return 0; }
    catch (const std::exception& e) { setError(e.what()); return -1; }
}

int spss_compute(SPSS_Dataset* ds, const char* target, const char* expression) {
    try {
        ComputeSpec s;
        s.target = target;
        s.expression = expression;
        TransformationEngine::compute(*ds->store, s);
        return 0;
    } catch (const std::exception& e) { setError(e.what()); return -1; }
}

int spss_recode_value(SPSS_Dataset* ds, const char* source, const char* target,
                      double from, double to) {
    try {
        RecodeSpec s;
        s.source = source;
        s.target = target;
        RecodeRule r;
        r.fromValue = from;
        r.toValue = to;
        s.rules.push_back(r);
        RecodeRule elseRule;
        elseRule.fromElse = true;
        elseRule.toCopy = true;
        s.rules.push_back(elseRule);
        TransformationEngine::recode(*ds->store, s);
        return 0;
    } catch (const std::exception& e) { setError(e.what()); return -1; }
}

int spss_sort(SPSS_Dataset* ds, const char* variable, int ascending) {
    try {
        TransformationEngine::sortCases(*ds->store, {{variable, ascending != 0}});
        return 0;
    } catch (const std::exception& e) { setError(e.what()); return -1; }
}

int spss_weight(SPSS_Dataset* ds, const char* variable) {
    try {
        TransformationEngine::weightBy(*ds->store, variable ? variable : "");
        return 0;
    } catch (const std::exception& e) { setError(e.what()); return -1; }
}

int spss_filter_select(SPSS_Dataset* ds, const char* expression) {
    try {
        TransformationEngine::selectIf(*ds->store, expression, true);
        return 0;
    } catch (const std::exception& e) { setError(e.what()); return -1; }
}

static SPSS_Output* wrap(OutputDocument doc) {
    auto* o = new SPSS_Output;
    o->doc = std::move(doc);
    o->htmlCache = o->doc.toHTML();
    return o;
}

SPSS_Output* spss_frequencies(const SPSS_Dataset* ds, const char* const* vars, size_t nvars) {
    FrequenciesOptions opt;
    for (size_t i = 0; i < nvars; ++i) opt.variables.push_back(vars[i]);
    opt.barchart = true;
    return wrap(ProcedureRunner::frequencies(*ds->store, opt));
}

SPSS_Output* spss_descriptives(const SPSS_Dataset* ds, const char* const* vars, size_t nvars) {
    DescriptivesOptions opt;
    for (size_t i = 0; i < nvars; ++i) opt.variables.push_back(vars[i]);
    return wrap(ProcedureRunner::descriptives(*ds->store, opt));
}

SPSS_Output* spss_crosstabs(const SPSS_Dataset* ds, const char* row, const char* col) {
    CrosstabsOptions opt;
    opt.rows = {row};
    opt.cols = {col};
    return wrap(ProcedureRunner::crosstabs(*ds->store, opt));
}

SPSS_Output* spss_ttest_onesample(const SPSS_Dataset* ds, const char* var, double testValue) {
    TTestOptions opt;
    opt.kind = TTestOptions::Kind::OneSample;
    opt.variables = {var};
    opt.testValue = testValue;
    return wrap(ProcedureRunner::ttest(*ds->store, opt));
}

SPSS_Output* spss_correlation(const SPSS_Dataset* ds, const char* const* vars, size_t nvars) {
    CorrelationsOptions opt;
    for (size_t i = 0; i < nvars; ++i) opt.variables.push_back(vars[i]);
    return wrap(ProcedureRunner::correlations(*ds->store, opt));
}

SPSS_Output* spss_regression(const SPSS_Dataset* ds, const char* dep, const char* const* indep, size_t n) {
    RegressionOptions opt;
    opt.dependent = dep;
    for (size_t i = 0; i < n; ++i) opt.independents.push_back(indep[i]);
    return wrap(ProcedureRunner::regression(*ds->store, opt));
}

SPSS_Output* spss_oneway(const SPSS_Dataset* ds, const char* dep, const char* factor) {
    OnewayOptions opt;
    opt.dependent = dep;
    opt.factor = factor;
    return wrap(ProcedureRunner::oneway(*ds->store, opt));
}

SPSS_Output* spss_glm(const SPSS_Dataset* ds, const char* dep, const char* const* factors, size_t nfactors,
                      const char* const* covariates, size_t ncov) {
    GLMOptions opt;
    opt.dependent = dep;
    for (size_t i = 0; i < nfactors; ++i) opt.factors.push_back(factors[i]);
    for (size_t i = 0; i < ncov; ++i) opt.covariates.push_back(covariates[i]);
    return wrap(ProcedureRunner::glmUnivariate(*ds->store, opt));
}

SPSS_Output* spss_factor(const SPSS_Dataset* ds, const char* const* vars, size_t nvars, int nfactors) {
    FactorOptions opt;
    for (size_t i = 0; i < nvars; ++i) opt.variables.push_back(vars[i]);
    opt.nFactors = nfactors;
    return wrap(ProcedureRunner::factorPCA(*ds->store, opt));
}

SPSS_Output* spss_cluster(const SPSS_Dataset* ds, const char* const* vars, size_t nvars, int k) {
    ClusterOptions opt;
    for (size_t i = 0; i < nvars; ++i) opt.variables.push_back(vars[i]);
    opt.k = k;
    return wrap(ProcedureRunner::quickCluster(*ds->store, opt));
}

SPSS_Output* spss_logistic(const SPSS_Dataset* ds, const char* dep, const char* const* indep, size_t n) {
    LogisticOptions opt;
    opt.dependent = dep;
    for (size_t i = 0; i < n; ++i) opt.independents.push_back(indep[i]);
    return wrap(ProcedureRunner::logistic(*ds->store, opt));
}

SPSS_Output* spss_km(const SPSS_Dataset* ds, const char* time, const char* status) {
    SurvivalKMOptions opt;
    opt.time = time;
    opt.status = status;
    return wrap(ProcedureRunner::kaplanMeier(*ds->store, opt));
}

SPSS_Output* spss_impute(const SPSS_Dataset* ds, const char* const* vars, size_t nvars, int m) {
    ImputationOptions opt;
    for (size_t i = 0; i < nvars; ++i) opt.variables.push_back(vars[i]);
    opt.m = m;
    return wrap(ProcedureRunner::multipleImputation(*ds->store, opt));
}

SPSS_Output* spss_bootstrap(const SPSS_Dataset* ds, const char* const* vars, size_t nvars, int samples) {
    BootstrapOptions opt;
    for (size_t i = 0; i < nvars; ++i) opt.variables.push_back(vars[i]);
    opt.samples = samples;
    return wrap(ProcedureRunner::bootstrapMeans(*ds->store, opt));
}

SPSS_Output* spss_forecast(const SPSS_Dataset* ds, const char* var, int periods, int window) {
    ForecastOptions opt;
    opt.variable = var;
    opt.periods = periods;
    opt.window = window;
    return wrap(ProcedureRunner::forecastSMA(*ds->store, opt));
}

SPSS_Output* spss_ctables(const SPSS_Dataset* ds, const char* row, const char* col, const char* summary) {
    CTablesOptions opt;
    opt.row = row;
    opt.column = col;
    if (summary && summary[0]) opt.summaryVar = summary;
    return wrap(ProcedureRunner::ctables(*ds->store, opt));
}

SPSS_Output* spss_tree(const SPSS_Dataset* ds, const char* dep, const char* predictor) {
    TreeOptions opt;
    opt.dependent = dep;
    opt.predictor = predictor;
    return wrap(ProcedureRunner::decisionTree(*ds->store, opt));
}

SPSS_Output* spss_exact(const SPSS_Dataset* ds, const char* row, const char* col) {
    ExactOptions opt;
    opt.row = row;
    opt.column = col;
    return wrap(ProcedureRunner::fisherExact(*ds->store, opt));
}

SPSS_Output* spss_neural(const SPSS_Dataset* ds, const char* dep, const char* const* indep, size_t n) {
    NeuralNetOptions opt;
    opt.dependent = dep;
    for (size_t i = 0; i < n; ++i) opt.independents.push_back(indep[i]);
    return wrap(ProcedureRunner::neuralNet(*ds->store, opt));
}

void spss_output_destroy(SPSS_Output* out) { delete out; }

size_t spss_output_item_count(const SPSS_Output* out) {
    return out ? out->doc.items().size() : 0;
}

const char* spss_output_html(const SPSS_Output* out) {
    return out ? out->htmlCache.c_str() : "";
}
