#pragma once

#include "DatasetStore.hpp"
#include <functional>
#include <string>
#include <vector>

namespace spss {

struct ComputeSpec {
    std::string target;
    /// Expression: supports + - * / () and variable names; numeric only MVP.
    std::string expression;
    std::string label;
};

struct RecodeRule {
    std::optional<double> fromValue;
    std::optional<std::pair<double, double>> fromRange;
    bool fromMissing{false};
    bool fromElse{false};
    double toValue{kSystemMissing};
    bool toCopy{false};
};

struct RecodeSpec {
    std::string source;
    std::string target; // same as source for into-same
    std::vector<RecodeRule> rules;
};

struct AutoRecodeSpec {
    std::string source;
    std::string target;
    bool groupIdentical{true};
};

struct RankSpec {
    std::string source;
    std::string target;
    bool ascending{true};
};

struct SortKey {
    std::string variable;
    bool ascending{true};
};

struct AggregateSpec {
    std::vector<std::string> breakVars;
    struct AggVar {
        std::string source;
        std::string function; // MEAN SUM N MIN MAX
        std::string target;
    };
    std::vector<AggVar> aggregations;
};

struct MergeSpec {
    enum class Mode { AddCases, AddVariables };
    Mode mode{Mode::AddCases};
    std::vector<std::string> keyVars; // for AddVariables match
};

class TransformationEngine {
public:
    static void compute(DatasetStore& ds, const ComputeSpec& spec);
    static void recode(DatasetStore& ds, const RecodeSpec& spec);
    static void autoRecode(DatasetStore& ds, const AutoRecodeSpec& spec);
    static void rank(DatasetStore& ds, const RankSpec& spec);
    static void sortCases(DatasetStore& ds, const std::vector<SortKey>& keys);
    static void selectIf(DatasetStore& ds, const std::string& expression, bool filterMode);
    static void weightBy(DatasetStore& ds, const std::string& variable);
    static void splitFile(DatasetStore& ds, const std::vector<std::string>& vars, bool layered);
    static std::unique_ptr<DatasetStore> aggregate(const DatasetStore& ds, const AggregateSpec& spec);
    static std::unique_ptr<DatasetStore> merge(
        const DatasetStore& a, const DatasetStore& b, const MergeSpec& spec);
    static std::unique_ptr<DatasetStore> varsToCases(
        const DatasetStore& ds,
        const std::vector<std::string>& stubVars,
        const std::string& indexName,
        const std::string& valueName);
    static std::unique_ptr<DatasetStore> casesToVars(
        const DatasetStore& ds,
        const std::vector<std::string>& idVars,
        const std::string& indexVar,
        const std::vector<std::string>& measureVars);

    /// Evaluate numeric expression against a row. Variables resolved by name.
    static double evalExpression(const DatasetStore& ds, size_t row, const std::string& expr);
};

} // namespace spss
