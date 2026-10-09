#pragma once

#include "Types.hpp"
#include <memory>
#include <stdexcept>

namespace spss {

/// Columnar in-memory store with SPSS dictionary overlay.
class DatasetStore {
public:
    DatasetStore() = default;
    explicit DatasetStore(std::string name) : name_(std::move(name)) {}

    const std::string& name() const { return name_; }
    void setName(std::string n) { name_ = std::move(n); }

    size_t caseCount() const { return caseCount_; }
    size_t variableCount() const { return variables_.size(); }

    const std::vector<Variable>& variables() const { return variables_; }
    Variable& variableAt(size_t i) { return variables_.at(i); }
    const Variable& variableAt(size_t i) const { return variables_.at(i); }

    int indexOf(const std::string& name) const;
    bool hasVariable(const std::string& name) const { return indexOf(name) >= 0; }

    DatasetFlags& flags() { return flags_; }
    const DatasetFlags& flags() const { return flags_; }

    size_t addVariable(Variable var, size_t casesHint = 0);
    void renameVariable(size_t index, const std::string& newName);
    void removeVariable(size_t index);

    void setCaseCount(size_t n);
    size_t addCases(size_t n);
    void deleteCase(size_t row);

    double getNumeric(size_t row, size_t col) const;
    void setNumeric(size_t row, size_t col, double value);

    const std::string& getString(size_t row, size_t col) const;
    void setString(size_t row, size_t col, std::string value);

    bool isMissing(size_t row, size_t col) const;
    bool isUserMissing(size_t row, size_t col) const;

    /// Effective weight for a case (1.0 if no weight).
    double caseWeight(size_t row) const;
    /// Whether case is selected under current filter (true if filter off).
    bool caseSelected(size_t row) const;

    std::unique_ptr<DatasetStore> clone() const;

    /// Undo snapshots (shallow metadata + full data copy).
    void pushUndo();
    bool canUndo() const { return !undoStack_.empty(); }
    bool canRedo() const { return !redoStack_.empty(); }
    void undo();
    void redo();

private:
    struct Snapshot {
        std::string name;
        std::vector<Variable> variables;
        std::vector<std::vector<double>> numericCols;
        std::vector<std::vector<std::string>> stringCols;
        size_t caseCount{0};
        DatasetFlags flags;
    };

    Snapshot capture() const;
    void restore(Snapshot s);

    std::string name_{"Dataset1"};
    std::vector<Variable> variables_;
    std::vector<std::vector<double>> numericCols_;
    std::vector<std::vector<std::string>> stringCols_;
    size_t caseCount_{0};
    DatasetFlags flags_;
    std::vector<Snapshot> undoStack_;
    std::vector<Snapshot> redoStack_;
    static constexpr size_t kMaxUndo = 50;
};

} // namespace spss
