#include "spss/Output.hpp"
#include <sstream>
#include <cmath>

namespace spss {

size_t OutputDocument::addItem(OutputItem item) {
    items_.push_back(std::move(item));
    return items_.size() - 1;
}

void OutputDocument::addToOutline(const std::string& section, size_t itemIndex, const std::string& title) {
    std::shared_ptr<OutputNode> sectionNode;
    for (auto& c : root_->children) {
        if (c->title == section) {
            sectionNode = c;
            break;
        }
    }
    if (!sectionNode) {
        sectionNode = std::make_shared<OutputNode>();
        sectionNode->title = section;
        sectionNode->kind = section;
        root_->children.push_back(sectionNode);
    }
    auto leaf = std::make_shared<OutputNode>();
    leaf->title = title;
    leaf->kind = section;
    leaf->itemIndices.push_back(itemIndex);
    sectionNode->children.push_back(leaf);
}

void OutputDocument::append(OutputDocument other) {
    size_t offset = items_.size();
    for (auto& it : other.items_) {
        items_.push_back(std::move(it));
    }
    for (auto& sec : other.root_->children) {
        for (auto& leaf : sec->children) {
            for (size_t idx : leaf->itemIndices) {
                addToOutline(sec->title, idx + offset, leaf->title);
            }
        }
    }
}

static std::string escapeHTML(const std::string& s) {
    std::string o;
    for (char c : s) {
        switch (c) {
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            default: o += c;
        }
    }
    return o;
}

std::string OutputDocument::toHTML() const {
    std::ostringstream oss;
    oss << "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>StatDesk Output</title>"
        << "<style>body{font-family:-apple-system,sans-serif;margin:24px}"
        << "table{border-collapse:collapse;margin:12px 0}th,td{border:1px solid #999;padding:4px 8px}"
        << "th{background:#f0f0f0}.warn{color:#a60}.note{color:#333}</style></head><body>";
    oss << "<h1>Output</h1>";
    for (const auto& item : items_) {
        if (auto* log = std::get_if<OutputLogEntry>(&item)) {
            oss << "<pre" << (log->error ? " style=\"color:red\"" : "") << ">"
                << escapeHTML(log->command) << "\n" << escapeHTML(log->message) << "</pre>";
        } else if (auto* note = std::get_if<OutputNote>(&item)) {
            oss << "<p class=\"note\">" << escapeHTML(note->text) << "</p>";
        } else if (auto* warn = std::get_if<OutputWarning>(&item)) {
            oss << "<p class=\"warn\">Warning: " << escapeHTML(warn->text) << "</p>";
        } else if (auto* t = std::get_if<PivotTable>(&item)) {
            oss << "<h2>" << escapeHTML(t->title) << "</h2><table><tr><th></th>";
            for (const auto& c : t->columnLabels) oss << "<th>" << escapeHTML(c) << "</th>";
            oss << "</tr>";
            for (size_t r = 0; r < t->rowLabels.size(); ++r) {
                oss << "<tr><th>" << escapeHTML(t->rowLabels[r]) << "</th>";
                for (size_t c = 0; c < t->columnLabels.size(); ++c) {
                    if (r >= t->cells.size() || c >= t->cells[r].size()) {
                        oss << "<td>.</td>";
                        continue;
                    }
                    const auto& cell = t->cells[r][c];
                    oss << "<td>" << escapeHTML(cell.display.empty()
                        ? (cell.isMissing ? "." : std::to_string(cell.value))
                        : cell.display) << "</td>";
                }
                oss << "</tr>";
            }
            oss << "</table>";
            for (const auto& f : t->footnotes) {
                oss << "<div><small>" << escapeHTML(f) << "</small></div>";
            }
        } else if (auto* ch = std::get_if<ChartSpec>(&item)) {
            oss << "<h2>" << escapeHTML(ch->title) << "</h2>";
            oss << "<p>Chart (" << escapeHTML(ch->type) << "): " << escapeHTML(ch->xLabel)
                << " × " << escapeHTML(ch->yLabel) << "</p><ul>";
            for (size_t i = 0; i < ch->categories.size() && i < ch->values.size(); ++i) {
                oss << "<li>" << escapeHTML(ch->categories[i]) << ": " << ch->values[i] << "</li>";
            }
            oss << "</ul>";
        }
    }
    oss << "</body></html>";
    return oss.str();
}

std::string OutputDocument::toCSVTables() const {
    std::ostringstream oss;
    for (const auto& item : items_) {
        if (auto* t = std::get_if<PivotTable>(&item)) {
            oss << "# " << t->title << "\n,";
            for (size_t c = 0; c < t->columnLabels.size(); ++c) {
                if (c) oss << ",";
                oss << t->columnLabels[c];
            }
            oss << "\n";
            for (size_t r = 0; r < t->rowLabels.size(); ++r) {
                oss << t->rowLabels[r];
                for (size_t c = 0; c < t->columnLabels.size(); ++c) {
                    oss << ",";
                    const auto& cell = t->cells[r][c];
                    if (!cell.isMissing) oss << cell.value;
                }
                oss << "\n";
            }
            oss << "\n";
        }
    }
    return oss.str();
}

} // namespace spss
