#include "spss/IO.hpp"
#include <fstream>
#include <sstream>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <unordered_set>

namespace spss {
namespace {

std::string trim(std::string s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

/// Collapse newlines/tabs inside cells (common in Google Forms exports) to spaces.
std::string collapseWS(std::string s) {
    std::string out;
    out.reserve(s.size());
    bool space = false;
    for (unsigned char uc : s) {
        char c = static_cast<char>(uc);
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!space && !out.empty()) {
                out.push_back(' ');
                space = true;
            }
        } else {
            out.push_back(c);
            space = false;
        }
    }
    return trim(out);
}

/// RFC 4180-ish CSV: quoted fields may contain commas, quotes (""), and newlines.
std::vector<std::vector<std::string>> parseCSVRecords(std::istream& in, char delim) {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> row;
    std::string cur;
    bool inQuotes = false;
    char c = 0;
    auto flushRow = [&]() {
        row.push_back(cur);
        cur.clear();
        bool empty = row.size() == 1 && row[0].empty();
        if (!empty) rows.push_back(std::move(row));
        row.clear();
    };
    while (in.get(c)) {
        if (c == '\r') continue; // CR of CRLF; LF ends the record when not quoted
        if (inQuotes) {
            if (c == '"') {
                if (in.peek() == '"') {
                    in.get();
                    cur.push_back('"');
                } else {
                    inQuotes = false;
                }
            } else {
                cur.push_back(c);
            }
            continue;
        }
        if (c == '"') {
            inQuotes = true;
        } else if (c == delim) {
            row.push_back(cur);
            cur.clear();
        } else if (c == '\n') {
            flushRow();
        } else {
            cur.push_back(c);
        }
    }
    if (inQuotes || !cur.empty() || !row.empty()) flushRow();
    return rows;
}

bool looksNumeric(const std::string& s) {
    if (s.empty()) return true;
    char* end = nullptr;
    std::strtod(s.c_str(), &end);
    return end && *end == '\0';
}

std::string sanitizeName(std::string s, size_t index) {
    for (char& c : s) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) c = '_';
    }
    // Collapse repeated underscores
    std::string compact;
    compact.reserve(s.size());
    bool prevUnderscore = false;
    for (char c : s) {
        if (c == '_') {
            if (!prevUnderscore && !compact.empty()) {
                compact.push_back('_');
                prevUnderscore = true;
            }
        } else {
            compact.push_back(c);
            prevUnderscore = false;
        }
    }
    while (!compact.empty() && compact.back() == '_') compact.pop_back();
    s = compact;
    if (s.size() > 48) s = s.substr(0, 48);
    if (s.empty() || std::isdigit(static_cast<unsigned char>(s[0]))) s = "V" + s;
    if (s.empty()) s = "VAR" + std::to_string(index + 1);
    return s;
}

std::string uniqueName(std::string base, size_t index, std::unordered_set<std::string>& used) {
    if (used.insert(base).second) return base;
    for (int n = 2; n < 10000; ++n) {
        std::string candidate = base + "_" + std::to_string(n);
        if (candidate.size() > 64) {
            candidate = base.substr(0, std::max<size_t>(1, 64 - 1 - std::to_string(n).size())) +
                        "_" + std::to_string(n);
        }
        if (used.insert(candidate).second) return candidate;
    }
    std::string fallback = "VAR" + std::to_string(index + 1);
    used.insert(fallback);
    return fallback;
}

std::unique_ptr<DatasetStore> fromRows(const std::vector<std::vector<std::string>>& rows,
                                       bool header, const std::string& name) {
    auto ds = std::make_unique<DatasetStore>(name);
    if (rows.empty()) return ds;
    size_t start = 0;
    std::vector<std::string> rawHeaders;
    if (header) {
        rawHeaders = rows[0];
        start = 1;
    } else {
        for (size_t i = 0; i < rows[0].size(); ++i) rawHeaders.push_back("VAR" + std::to_string(i + 1));
    }
    // Widen to max columns seen (ragged survey exports)
    size_t ncols = rawHeaders.size();
    for (size_t r = start; r < rows.size(); ++r) ncols = std::max(ncols, rows[r].size());
    while (rawHeaders.size() < ncols) {
        rawHeaders.push_back("VAR" + std::to_string(rawHeaders.size() + 1));
    }

    std::vector<std::string> labels(ncols);
    std::vector<std::string> names(ncols);
    std::unordered_set<std::string> used;
    for (size_t c = 0; c < ncols; ++c) {
        labels[c] = collapseWS(rawHeaders[c]);
        names[c] = uniqueName(sanitizeName(labels[c], c), c, used);
    }

    std::vector<bool> isNum(ncols, true);
    for (size_t r = start; r < rows.size(); ++r) {
        for (size_t c = 0; c < ncols && c < rows[r].size(); ++c) {
            std::string cell = trim(rows[r][c]);
            if (!cell.empty() && !looksNumeric(cell)) isNum[c] = false;
        }
    }
    for (size_t c = 0; c < ncols; ++c) {
        Variable v;
        v.name = names[c];
        v.label = labels[c];
        v.type = isNum[c] ? VarType::Numeric : VarType::String;
        if (!isNum[c]) { v.align = Align::Left; v.measure = Measure::Nominal; }
        ds->addVariable(v);
    }
    size_t ncases = rows.size() > start ? rows.size() - start : 0;
    ds->setCaseCount(ncases);
    for (size_t r = 0; r < ncases; ++r) {
        const auto& row = rows[start + r];
        for (size_t c = 0; c < ncols; ++c) {
            std::string cell = c < row.size() ? trim(row[c]) : "";
            if (isNum[c]) {
                if (cell.empty()) ds->setNumeric(r, c, kSystemMissing);
                else ds->setNumeric(r, c, std::strtod(cell.c_str(), nullptr));
            } else {
                ds->setString(r, c, collapseWS(cell));
            }
        }
    }
    return ds;
}

} // namespace

std::unique_ptr<DatasetStore> DataIO::readCSV(const std::string& path, const ImportOptions& opt) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Cannot open CSV: " + path);
    auto rows = parseCSVRecords(in, opt.delimiter);
    auto slash = path.find_last_of("/\\");
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    return fromRows(rows, opt.firstRowHeader, name);
}

void DataIO::writeCSV(const DatasetStore& ds, const std::string& path, const ImportOptions& opt) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("Cannot write CSV: " + path);
    for (size_t c = 0; c < ds.variableCount(); ++c) {
        if (c) out << opt.delimiter;
        out << ds.variableAt(c).name;
    }
    out << "\n";
    for (size_t r = 0; r < ds.caseCount(); ++r) {
        for (size_t c = 0; c < ds.variableCount(); ++c) {
            if (c) out << opt.delimiter;
            if (ds.variableAt(c).type == VarType::String) {
                out << '"' << ds.getString(r, c) << '"';
            } else if (!ds.isMissing(r, c)) {
                out << ds.getNumeric(r, c);
            }
        }
        out << "\n";
    }
}

std::unique_ptr<DatasetStore> DataIO::readExcel(const std::string& path, const ImportOptions& opt) {
    // MVP: treat as CSV/TSV if extension suggests; otherwise try comma CSV.
    ImportOptions o = opt;
    if (path.size() > 4 && path.substr(path.size() - 4) == ".tsv") o.delimiter = '\t';
    return readCSV(path, o);
}

void DataIO::writeExcelCSV(const DatasetStore& ds, const std::string& path) {
    writeCSV(ds, path, {});
}

// Portable SAV-like format for this app (magic SPSSMAC1). Independent of IBM binary layout.
static constexpr char kMagic[8] = {'S','P','S','S','M','A','C','1'};

void DataIO::writeSAV(const DatasetStore& ds, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot write SAV: " + path);
    out.write(kMagic, 8);
    uint64_t nvar = ds.variableCount();
    uint64_t ncase = ds.caseCount();
    out.write(reinterpret_cast<const char*>(&nvar), 8);
    out.write(reinterpret_cast<const char*>(&ncase), 8);
    auto writeStr = [&](const std::string& s) {
        uint32_t n = static_cast<uint32_t>(s.size());
        out.write(reinterpret_cast<const char*>(&n), 4);
        out.write(s.data(), n);
    };
    for (size_t c = 0; c < ds.variableCount(); ++c) {
        const auto& v = ds.variableAt(c);
        writeStr(v.name);
        writeStr(v.label);
        writeStr(v.printFormat);
        uint8_t type = static_cast<uint8_t>(v.type);
        uint8_t measure = static_cast<uint8_t>(v.measure);
        uint8_t align = static_cast<uint8_t>(v.align);
        uint8_t role = static_cast<uint8_t>(v.role);
        int32_t width = v.width, decimals = v.decimals, columns = v.columns;
        out.write(reinterpret_cast<const char*>(&type), 1);
        out.write(reinterpret_cast<const char*>(&measure), 1);
        out.write(reinterpret_cast<const char*>(&align), 1);
        out.write(reinterpret_cast<const char*>(&role), 1);
        out.write(reinterpret_cast<const char*>(&width), 4);
        out.write(reinterpret_cast<const char*>(&decimals), 4);
        out.write(reinterpret_cast<const char*>(&columns), 4);
        uint32_t nlab = static_cast<uint32_t>(v.valueLabels.size());
        out.write(reinterpret_cast<const char*>(&nlab), 4);
        for (const auto& vl : v.valueLabels) {
            out.write(reinterpret_cast<const char*>(&vl.numericKey), 8);
            writeStr(vl.label);
        }
        uint32_t nmiss = static_cast<uint32_t>(v.missing.discreteNumeric.size());
        out.write(reinterpret_cast<const char*>(&nmiss), 4);
        for (double m : v.missing.discreteNumeric) out.write(reinterpret_cast<const char*>(&m), 8);
    }
    for (size_t c = 0; c < ds.variableCount(); ++c) {
        if (ds.variableAt(c).type == VarType::String) {
            for (size_t r = 0; r < ds.caseCount(); ++r) writeStr(ds.getString(r, c));
        } else {
            for (size_t r = 0; r < ds.caseCount(); ++r) {
                double v = ds.getNumeric(r, c);
                out.write(reinterpret_cast<const char*>(&v), 8);
            }
        }
    }
}

std::unique_ptr<DatasetStore> DataIO::readSAV(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open SAV: " + path);
    char magic[8];
    in.read(magic, 8);
    if (std::memcmp(magic, kMagic, 8) != 0) {
        throw std::runtime_error("Unsupported SAV format (expected StatDesk SPSSMAC1); IBM binary SAV reader planned");
    }
    uint64_t nvar = 0, ncase = 0;
    in.read(reinterpret_cast<char*>(&nvar), 8);
    in.read(reinterpret_cast<char*>(&ncase), 8);
    auto readStr = [&]() {
        uint32_t n = 0;
        in.read(reinterpret_cast<char*>(&n), 4);
        std::string s(n, '\0');
        if (n) in.read(s.data(), n);
        return s;
    };
    auto slash = path.find_last_of("/\\");
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    auto ds = std::make_unique<DatasetStore>(name);
    for (uint64_t c = 0; c < nvar; ++c) {
        Variable v;
        v.name = readStr();
        v.label = readStr();
        v.printFormat = readStr();
        uint8_t type, measure, align, role;
        in.read(reinterpret_cast<char*>(&type), 1);
        in.read(reinterpret_cast<char*>(&measure), 1);
        in.read(reinterpret_cast<char*>(&align), 1);
        in.read(reinterpret_cast<char*>(&role), 1);
        int32_t width, decimals, columns;
        in.read(reinterpret_cast<char*>(&width), 4);
        in.read(reinterpret_cast<char*>(&decimals), 4);
        in.read(reinterpret_cast<char*>(&columns), 4);
        v.type = static_cast<VarType>(type);
        v.measure = static_cast<Measure>(measure);
        v.align = static_cast<Align>(align);
        v.role = static_cast<Role>(role);
        v.width = width; v.decimals = decimals; v.columns = columns;
        uint32_t nlab = 0;
        in.read(reinterpret_cast<char*>(&nlab), 4);
        for (uint32_t i = 0; i < nlab; ++i) {
            ValueLabel vl;
            in.read(reinterpret_cast<char*>(&vl.numericKey), 8);
            vl.label = readStr();
            v.valueLabels.push_back(vl);
        }
        uint32_t nmiss = 0;
        in.read(reinterpret_cast<char*>(&nmiss), 4);
        for (uint32_t i = 0; i < nmiss; ++i) {
            double m; in.read(reinterpret_cast<char*>(&m), 8);
            v.missing.discreteNumeric.push_back(m);
        }
        ds->addVariable(v);
    }
    ds->setCaseCount(static_cast<size_t>(ncase));
    for (uint64_t c = 0; c < nvar; ++c) {
        if (ds->variableAt(static_cast<size_t>(c)).type == VarType::String) {
            for (uint64_t r = 0; r < ncase; ++r) ds->setString(static_cast<size_t>(r), static_cast<size_t>(c), readStr());
        } else {
            for (uint64_t r = 0; r < ncase; ++r) {
                double v; in.read(reinterpret_cast<char*>(&v), 8);
                ds->setNumeric(static_cast<size_t>(r), static_cast<size_t>(c), v);
            }
        }
    }
    return ds;
}

} // namespace spss
