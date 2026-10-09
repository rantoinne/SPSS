#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <cmath>

namespace spss {

inline constexpr double kSystemMissing = std::numeric_limits<double>::quiet_NaN();

inline bool isSystemMissing(double v) {
    return std::isnan(v);
}

enum class VarType : uint8_t {
    Numeric = 0,
    String = 1,
    Date = 2
};

enum class Measure : uint8_t {
    Scale = 0,
    Ordinal = 1,
    Nominal = 2
};

enum class Align : uint8_t {
    Left = 0,
    Right = 1,
    Center = 2
};

enum class Role : uint8_t {
    Input = 0,
    Target = 1,
    Both = 2,
    None = 3,
    Partition = 4,
    Split = 5,
    Frequency = 6,
    RecordID = 7
};

struct ValueLabel {
    double numericKey{0};
    std::string stringKey;
    std::string label;
    bool isString{false};
};

struct MissingSpec {
    std::vector<double> discreteNumeric;
    std::optional<std::pair<double, double>> range;
    std::vector<std::string> discreteString;
};

struct Variable {
    std::string name;
    VarType type{VarType::Numeric};
    int width{8};
    int decimals{2};
    std::string label;
    std::vector<ValueLabel> valueLabels;
    MissingSpec missing;
    int columns{8};
    Align align{Align::Right};
    Measure measure{Measure::Scale};
    Role role{Role::Input};
    std::string printFormat{"F8.2"};
    std::string writeFormat{"F8.2"};
};

struct DatasetFlags {
    bool filterOn{false};
    bool weightOn{false};
    bool splitOn{false};
    std::string filterVariable;
    std::string weightVariable;
    std::vector<std::string> splitVariables;
};

} // namespace spss
