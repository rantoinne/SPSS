#include "spss/DatasetStore.hpp"
#include <algorithm>

namespace spss {

int DatasetStore::indexOf(const std::string& name) const {
    for (size_t i = 0; i < variables_.size(); ++i) {
        if (variables_[i].name == name) return static_cast<int>(i);
    }
    return -1;
}

size_t DatasetStore::addVariable(Variable var, size_t casesHint) {
    if (hasVariable(var.name)) {
        throw std::runtime_error("Variable already exists: " + var.name);
    }
    size_t cases = casesHint ? casesHint : caseCount_;
    if (var.type == VarType::String) {
        stringCols_.emplace_back(cases, "");
        numericCols_.emplace_back(); // placeholder unused
    } else {
        numericCols_.emplace_back(cases, kSystemMissing);
        stringCols_.emplace_back();
    }
    variables_.push_back(std::move(var));
    if (cases > caseCount_) caseCount_ = cases;
    return variables_.size() - 1;
}

void DatasetStore::renameVariable(size_t index, const std::string& newName) {
    if (hasVariable(newName) && static_cast<size_t>(indexOf(newName)) != index) {
        throw std::runtime_error("Duplicate variable name");
    }
    variables_.at(index).name = newName;
}

void DatasetStore::removeVariable(size_t index) {
    variables_.erase(variables_.begin() + static_cast<long>(index));
    numericCols_.erase(numericCols_.begin() + static_cast<long>(index));
    stringCols_.erase(stringCols_.begin() + static_cast<long>(index));
}

void DatasetStore::setCaseCount(size_t n) {
    caseCount_ = n;
    for (size_t i = 0; i < variables_.size(); ++i) {
        if (variables_[i].type == VarType::String) {
            stringCols_[i].resize(n);
        } else {
            numericCols_[i].resize(n, kSystemMissing);
        }
    }
}

size_t DatasetStore::addCases(size_t n) {
    size_t start = caseCount_;
    setCaseCount(caseCount_ + n);
    return start;
}

void DatasetStore::deleteCase(size_t row) {
    if (row >= caseCount_) return;
    for (size_t i = 0; i < variables_.size(); ++i) {
        if (variables_[i].type == VarType::String) {
            stringCols_[i].erase(stringCols_[i].begin() + static_cast<long>(row));
        } else {
            numericCols_[i].erase(numericCols_[i].begin() + static_cast<long>(row));
        }
    }
    --caseCount_;
}

double DatasetStore::getNumeric(size_t row, size_t col) const {
    if (variables_.at(col).type == VarType::String) return kSystemMissing;
    return numericCols_.at(col).at(row);
}

void DatasetStore::setNumeric(size_t row, size_t col, double value) {
    if (variables_.at(col).type == VarType::String) {
        throw std::runtime_error("Cannot set numeric on string variable");
    }
    numericCols_.at(col).at(row) = value;
}

const std::string& DatasetStore::getString(size_t row, size_t col) const {
    static const std::string empty;
    if (variables_.at(col).type != VarType::String) return empty;
    return stringCols_.at(col).at(row);
}

void DatasetStore::setString(size_t row, size_t col, std::string value) {
    if (variables_.at(col).type != VarType::String) {
        throw std::runtime_error("Cannot set string on numeric variable");
    }
    stringCols_.at(col).at(row) = std::move(value);
}

bool DatasetStore::isUserMissing(size_t row, size_t col) const {
    const auto& v = variables_.at(col);
    if (v.type == VarType::String) {
        const auto& s = getString(row, col);
        for (const auto& m : v.missing.discreteString) {
            if (m == s) return true;
        }
        return false;
    }
    double x = getNumeric(row, col);
    if (isSystemMissing(x)) return false;
    for (double m : v.missing.discreteNumeric) {
        if (x == m) return true;
    }
    if (v.missing.range) {
        if (x >= v.missing.range->first && x <= v.missing.range->second) return true;
    }
    return false;
}

bool DatasetStore::isMissing(size_t row, size_t col) const {
    if (variables_.at(col).type == VarType::String) {
        return getString(row, col).empty() || isUserMissing(row, col);
    }
    return isSystemMissing(getNumeric(row, col)) || isUserMissing(row, col);
}

double DatasetStore::caseWeight(size_t row) const {
    if (!flags_.weightOn || flags_.weightVariable.empty()) return 1.0;
    int idx = indexOf(flags_.weightVariable);
    if (idx < 0) return 1.0;
    double w = getNumeric(row, static_cast<size_t>(idx));
    if (isSystemMissing(w) || w <= 0) return 0.0;
    return w;
}

bool DatasetStore::caseSelected(size_t row) const {
    if (!flags_.filterOn || flags_.filterVariable.empty()) return true;
    int idx = indexOf(flags_.filterVariable);
    if (idx < 0) return true;
    double f = getNumeric(row, static_cast<size_t>(idx));
    return !isSystemMissing(f) && f != 0.0;
}

DatasetStore::Snapshot DatasetStore::capture() const {
    return Snapshot{name_, variables_, numericCols_, stringCols_, caseCount_, flags_};
}

void DatasetStore::restore(Snapshot s) {
    name_ = std::move(s.name);
    variables_ = std::move(s.variables);
    numericCols_ = std::move(s.numericCols);
    stringCols_ = std::move(s.stringCols);
    caseCount_ = s.caseCount;
    flags_ = std::move(s.flags);
}

std::unique_ptr<DatasetStore> DatasetStore::clone() const {
    auto d = std::make_unique<DatasetStore>();
    d->restore(capture());
    return d;
}

void DatasetStore::pushUndo() {
    undoStack_.push_back(capture());
    if (undoStack_.size() > kMaxUndo) undoStack_.erase(undoStack_.begin());
    redoStack_.clear();
}

void DatasetStore::undo() {
    if (undoStack_.empty()) return;
    redoStack_.push_back(capture());
    restore(std::move(undoStack_.back()));
    undoStack_.pop_back();
}

void DatasetStore::redo() {
    if (redoStack_.empty()) return;
    undoStack_.push_back(capture());
    restore(std::move(redoStack_.back()));
    redoStack_.pop_back();
}

} // namespace spss
