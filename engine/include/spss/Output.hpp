#pragma once

#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <optional>

namespace spss {

struct PivotCell {
    double value{0};
    std::string display;
    bool isMissing{false};
};

struct PivotTable {
    std::string title;
    std::vector<std::string> rowLabels;
    std::vector<std::string> columnLabels;
    std::vector<std::vector<PivotCell>> cells; // [row][col]
    std::vector<std::string> footnotes;
    std::string notes;
};

struct ChartSpec {
    std::string title;
    std::string type; // bar, histogram, scatter, box, line
    std::vector<std::string> categories;
    std::vector<double> values;
    std::vector<double> values2;
    std::string xLabel;
    std::string yLabel;
};

struct OutputNote {
    std::string text;
};

struct OutputWarning {
    std::string text;
};

struct OutputLogEntry {
    std::string command;
    std::string message;
    bool error{false};
};

using OutputItem = std::variant<OutputLogEntry, OutputNote, OutputWarning, PivotTable, ChartSpec>;

struct OutputNode {
    std::string title;
    std::string kind; // Log, Notes, Warnings, Tables, Charts, Folder
    std::vector<size_t> itemIndices;
    std::vector<std::shared_ptr<OutputNode>> children;
};

class OutputDocument {
public:
    OutputDocument() {
        root_ = std::make_shared<OutputNode>();
        root_->title = "Output";
        root_->kind = "Folder";
    }

    size_t addItem(OutputItem item);
    void addToOutline(const std::string& section, size_t itemIndex, const std::string& title);

    const std::vector<OutputItem>& items() const { return items_; }
    std::shared_ptr<OutputNode> root() const { return root_; }

    void append(OutputDocument other);

    std::string toHTML() const;
    std::string toCSVTables() const;

private:
    std::vector<OutputItem> items_;
    std::shared_ptr<OutputNode> root_;
};

} // namespace spss
