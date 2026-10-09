#pragma once

#include "DatasetStore.hpp"
#include "Output.hpp"
#include <string>
#include <vector>

namespace spss {

struct FrequenciesOptions {
    std::vector<std::string> variables;
    bool statistics{true};
    bool barchart{false};
};

struct DescriptivesOptions {
    std::vector<std::string> variables;
    bool mean{true};
    bool stddev{true};
    bool min{true};
    bool max{true};
    bool n{true};
    bool kurtosis{false};
    bool skewness{false};
};

struct ExploreOptions {
    std::vector<std::string> dependents;
    std::vector<std::string> factors;
};

struct CrosstabsOptions {
    std::vector<std::string> rows;
    std::vector<std::string> cols;
    bool chiSquare{true};
    bool expected{false};
    bool percentagesRow{false};
    bool percentagesCol{false};
};

struct TTestOptions {
    enum class Kind { OneSample, Independent, Paired };
    Kind kind{Kind::OneSample};
    std::vector<std::string> variables;
    std::string groupVariable;
    double group1{1};
    double group2{2};
    std::string pairedVar1;
    std::string pairedVar2;
    double testValue{0};
};

struct OnewayOptions {
    std::string dependent;
    std::string factor;
};

struct CorrelationsOptions {
    std::vector<std::string> variables;
    bool pearson{true};
    bool spearman{false};
    bool twotailed{true};
};

struct RegressionOptions {
    std::string dependent;
    std::vector<std::string> independents;
    bool residuals{true};
    bool collinearity{false};
};

struct NParOptions {
    enum class Test { Wilcoxon, MannWhitney, KruskalWallis, Sign };
    Test test{Test::Wilcoxon};
    std::vector<std::string> variables;
    std::string groupVariable;
};

struct GLMOptions {
    std::string dependent;
    std::vector<std::string> factors;
    std::vector<std::string> covariates;
};

struct FactorOptions {
    std::vector<std::string> variables;
    int nFactors{0}; // 0 = min(p, cases-1) capped
};

struct ClusterOptions {
    std::vector<std::string> variables;
    int k{2};
    int maxIter{25};
};

struct LogisticOptions {
    std::string dependent;
    std::vector<std::string> independents;
    int maxIter{30};
};

struct SurvivalKMOptions {
    std::string time;
    std::string status; // event=1
    std::string factor; // optional strata
};

struct ImputationOptions {
    std::vector<std::string> variables;
    int m{5};
};

struct BootstrapOptions {
    std::vector<std::string> variables;
    int samples{200};
    unsigned seed{42};
};

struct ForecastOptions {
    std::string variable;
    int periods{3};
    int window{3};
};

struct CTablesOptions {
    std::string row;
    std::string column;
    std::string summaryVar; // optional mean of this var in cells
};

struct TreeOptions {
    std::string dependent;
    std::string predictor;
    int minLeaf{2};
};

struct ExactOptions {
    std::string row;
    std::string column;
};

struct NeuralNetOptions {
    std::string dependent;
    std::vector<std::string> independents;
    int epochs{200};
    double lr{0.1};
};

class ProcedureRunner {
public:
    static OutputDocument frequencies(const DatasetStore& ds, const FrequenciesOptions& opt);
    static OutputDocument descriptives(const DatasetStore& ds, const DescriptivesOptions& opt);
    static OutputDocument explore(const DatasetStore& ds, const ExploreOptions& opt);
    static OutputDocument crosstabs(const DatasetStore& ds, const CrosstabsOptions& opt);
    static OutputDocument ttest(const DatasetStore& ds, const TTestOptions& opt);
    static OutputDocument oneway(const DatasetStore& ds, const OnewayOptions& opt);
    static OutputDocument correlations(const DatasetStore& ds, const CorrelationsOptions& opt);
    static OutputDocument regression(const DatasetStore& ds, const RegressionOptions& opt);
    static OutputDocument npar(const DatasetStore& ds, const NParOptions& opt);

    // Phase 6–7 advanced / Complete MVP modules
    static OutputDocument glmUnivariate(const DatasetStore& ds, const GLMOptions& opt);
    static OutputDocument factorPCA(const DatasetStore& ds, const FactorOptions& opt);
    static OutputDocument quickCluster(const DatasetStore& ds, const ClusterOptions& opt);
    static OutputDocument logistic(const DatasetStore& ds, const LogisticOptions& opt);
    static OutputDocument kaplanMeier(const DatasetStore& ds, const SurvivalKMOptions& opt);
    static OutputDocument multipleImputation(const DatasetStore& ds, const ImputationOptions& opt);
    static OutputDocument bootstrapMeans(const DatasetStore& ds, const BootstrapOptions& opt);
    static OutputDocument forecastSMA(const DatasetStore& ds, const ForecastOptions& opt);
    static OutputDocument ctables(const DatasetStore& ds, const CTablesOptions& opt);
    static OutputDocument decisionTree(const DatasetStore& ds, const TreeOptions& opt);
    static OutputDocument fisherExact(const DatasetStore& ds, const ExactOptions& opt);
    static OutputDocument neuralNet(const DatasetStore& ds, const NeuralNetOptions& opt);
};

} // namespace spss
