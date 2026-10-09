#include "spss/Transform.hpp"
#include <cctype>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <algorithm>

namespace spss {
namespace {

struct Tok {
    enum Kind { Num, Ident, Op, End } kind;
    double num{0};
    std::string text;
};

class Lexer {
public:
    explicit Lexer(std::string s) : s_(std::move(s)) {}
    Tok next() {
        while (i_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[i_]))) ++i_;
        if (i_ >= s_.size()) return {Tok::End};
        char c = s_[i_];
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            size_t start = i_;
            while (i_ < s_.size() && (std::isdigit(static_cast<unsigned char>(s_[i_])) || s_[i_] == '.' ||
                                      s_[i_] == 'e' || s_[i_] == 'E' || s_[i_] == '+' || s_[i_] == '-')) {
                if ((s_[i_] == '+' || s_[i_] == '-') && i_ > start && s_[i_-1] != 'e' && s_[i_-1] != 'E') break;
                ++i_;
            }
            return {Tok::Num, std::stod(s_.substr(start, i_ - start)), s_.substr(start, i_ - start)};
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t start = i_;
            while (i_ < s_.size() && (std::isalnum(static_cast<unsigned char>(s_[i_])) || s_[i_] == '_')) ++i_;
            return {Tok::Ident, 0, s_.substr(start, i_ - start)};
        }
        ++i_;
        return {Tok::Op, 0, std::string(1, c)};
    }
private:
    std::string s_;
    size_t i_{0};
};

class Parser {
public:
    Parser(const DatasetStore& ds, size_t row, std::string expr)
        : ds_(ds), row_(row), lex_(std::move(expr)) { tok_ = lex_.next(); }

    double parse() {
        double v = parseCmp();
        if (tok_.kind != Tok::End) throw std::runtime_error("Unexpected token in expression");
        return v;
    }

private:
    double parseCmp() {
        double left = parseAdd();
        while (tok_.kind == Tok::Op && (tok_.text == "=" || tok_.text == "<" || tok_.text == ">" ||
                                        tok_.text == "!" )) {
            std::string op = tok_.text;
            if (op == "!" || op == "<" || op == ">") {
                advance();
                if (tok_.kind == Tok::Op && tok_.text == "=") { op += "="; advance(); }
            } else {
                advance();
            }
            double right = parseAdd();
            if (isSystemMissing(left) || isSystemMissing(right)) return kSystemMissing;
            if (op == "=" || op == "==") left = left == right ? 1 : 0;
            else if (op == "!=") left = left != right ? 1 : 0;
            else if (op == "<") left = left < right ? 1 : 0;
            else if (op == ">") left = left > right ? 1 : 0;
            else if (op == "<=") left = left <= right ? 1 : 0;
            else if (op == ">=") left = left >= right ? 1 : 0;
        }
        return left;
    }
    double parseAdd() {
        double left = parseMul();
        while (tok_.kind == Tok::Op && (tok_.text == "+" || tok_.text == "-")) {
            std::string op = tok_.text; advance();
            double right = parseMul();
            if (isSystemMissing(left) || isSystemMissing(right)) return kSystemMissing;
            left = op == "+" ? left + right : left - right;
        }
        return left;
    }
    double parseMul() {
        double left = parseUnary();
        while (tok_.kind == Tok::Op && (tok_.text == "*" || tok_.text == "/")) {
            std::string op = tok_.text; advance();
            double right = parseUnary();
            if (isSystemMissing(left) || isSystemMissing(right)) return kSystemMissing;
            if (op == "/" && right == 0) return kSystemMissing;
            left = op == "*" ? left * right : left / right;
        }
        return left;
    }
    double parseUnary() {
        if (tok_.kind == Tok::Op && tok_.text == "-") { advance(); return -parseUnary(); }
        if (tok_.kind == Tok::Op && tok_.text == "+") { advance(); return parseUnary(); }
        return parsePrimary();
    }
    double parsePrimary() {
        if (tok_.kind == Tok::Num) { double v = tok_.num; advance(); return v; }
        if (tok_.kind == Tok::Ident) {
            std::string name = tok_.text; advance();
            if (name == "MISSING" || name == "SYSMIS") return kSystemMissing;
            int idx = ds_.indexOf(name);
            if (idx < 0) throw std::runtime_error("Unknown variable: " + name);
            if (ds_.variableAt(static_cast<size_t>(idx)).type == VarType::String) {
                throw std::runtime_error("String variable in numeric expression: " + name);
            }
            if (ds_.isMissing(row_, static_cast<size_t>(idx))) return kSystemMissing;
            return ds_.getNumeric(row_, static_cast<size_t>(idx));
        }
        if (tok_.kind == Tok::Op && tok_.text == "(") {
            advance();
            double v = parseCmp();
            if (!(tok_.kind == Tok::Op && tok_.text == ")")) throw std::runtime_error("Expected )");
            advance();
            return v;
        }
        throw std::runtime_error("Invalid expression");
    }
    void advance() { tok_ = lex_.next(); }

    const DatasetStore& ds_;
    size_t row_;
    Lexer lex_;
    Tok tok_;
};

} // namespace

double TransformationEngine::evalExpression(const DatasetStore& ds, size_t row, const std::string& expr) {
    Parser p(ds, row, expr);
    return p.parse();
}

void TransformationEngine::compute(DatasetStore& ds, const ComputeSpec& spec) {
    ds.pushUndo();
    int idx = ds.indexOf(spec.target);
    if (idx < 0) {
        Variable v;
        v.name = spec.target;
        v.type = VarType::Numeric;
        v.label = spec.label;
        idx = static_cast<int>(ds.addVariable(std::move(v)));
    }
    for (size_t r = 0; r < ds.caseCount(); ++r) {
        try {
            ds.setNumeric(r, static_cast<size_t>(idx), evalExpression(ds, r, spec.expression));
        } catch (...) {
            ds.setNumeric(r, static_cast<size_t>(idx), kSystemMissing);
        }
    }
}

void TransformationEngine::recode(DatasetStore& ds, const RecodeSpec& spec) {
    ds.pushUndo();
    int src = ds.indexOf(spec.source);
    if (src < 0) throw std::runtime_error("Source variable not found");
    int tgt = ds.indexOf(spec.target);
    if (tgt < 0) {
        Variable v = ds.variableAt(static_cast<size_t>(src));
        v.name = spec.target;
        tgt = static_cast<int>(ds.addVariable(std::move(v)));
    }
    for (size_t r = 0; r < ds.caseCount(); ++r) {
        double x = ds.getNumeric(r, static_cast<size_t>(src));
        bool matched = false;
        double out = x;
        for (const auto& rule : spec.rules) {
            bool hit = false;
            if (rule.fromElse) { hit = !matched; }
            else if (rule.fromMissing) { hit = ds.isMissing(r, static_cast<size_t>(src)); }
            else if (rule.fromRange) {
                if (!isSystemMissing(x) && x >= rule.fromRange->first && x <= rule.fromRange->second) hit = true;
            } else if (rule.fromValue) {
                if (!isSystemMissing(x) && x == *rule.fromValue) hit = true;
            }
            if (hit) {
                matched = true;
                out = rule.toCopy ? x : rule.toValue;
                break;
            }
        }
        ds.setNumeric(r, static_cast<size_t>(tgt), out);
    }
}

void TransformationEngine::autoRecode(DatasetStore& ds, const AutoRecodeSpec& spec) {
    ds.pushUndo();
    int src = ds.indexOf(spec.source);
    if (src < 0) throw std::runtime_error("Source not found");
    int tgt = ds.indexOf(spec.target);
    if (tgt < 0) {
        Variable v;
        v.name = spec.target;
        v.type = VarType::Numeric;
        v.measure = Measure::Nominal;
        tgt = static_cast<int>(ds.addVariable(std::move(v)));
    }
    std::map<std::string, double> map;
    double next = 1;
    bool isStr = ds.variableAt(static_cast<size_t>(src)).type == VarType::String;
    for (size_t r = 0; r < ds.caseCount(); ++r) {
        std::string key;
        if (isStr) key = ds.getString(r, static_cast<size_t>(src));
        else {
            double x = ds.getNumeric(r, static_cast<size_t>(src));
            if (isSystemMissing(x)) { ds.setNumeric(r, static_cast<size_t>(tgt), kSystemMissing); continue; }
            key = std::to_string(x);
        }
        if (!map.count(key)) {
            map[key] = next++;
            ValueLabel vl;
            vl.numericKey = map[key];
            vl.label = key;
            ds.variableAt(static_cast<size_t>(tgt)).valueLabels.push_back(vl);
        }
        ds.setNumeric(r, static_cast<size_t>(tgt), map[key]);
    }
}

void TransformationEngine::rank(DatasetStore& ds, const RankSpec& spec) {
    ds.pushUndo();
    int src = ds.indexOf(spec.source);
    if (src < 0) throw std::runtime_error("Source not found");
    int tgt = ds.indexOf(spec.target);
    if (tgt < 0) {
        Variable v;
        v.name = spec.target;
        v.type = VarType::Numeric;
        tgt = static_cast<int>(ds.addVariable(std::move(v)));
    }
    std::vector<size_t> idx;
    for (size_t r = 0; r < ds.caseCount(); ++r) {
        if (ds.caseSelected(r) && !ds.isMissing(r, static_cast<size_t>(src))) idx.push_back(r);
    }
    std::sort(idx.begin(), idx.end(), [&](size_t a, size_t b) {
        double va = ds.getNumeric(a, static_cast<size_t>(src));
        double vb = ds.getNumeric(b, static_cast<size_t>(src));
        return spec.ascending ? va < vb : va > vb;
    });
    for (size_t r = 0; r < ds.caseCount(); ++r) {
        ds.setNumeric(r, static_cast<size_t>(tgt), kSystemMissing);
    }
    for (size_t i = 0; i < idx.size();) {
        size_t j = i;
        double val = ds.getNumeric(idx[i], static_cast<size_t>(src));
        while (j < idx.size() && ds.getNumeric(idx[j], static_cast<size_t>(src)) == val) ++j;
        double avgRank = (static_cast<double>(i + 1) + static_cast<double>(j)) / 2.0;
        for (size_t k = i; k < j; ++k) ds.setNumeric(idx[k], static_cast<size_t>(tgt), avgRank);
        i = j;
    }
}

void TransformationEngine::sortCases(DatasetStore& ds, const std::vector<SortKey>& keys) {
    if (keys.empty()) return;
    ds.pushUndo();
    std::vector<size_t> order(ds.caseCount());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        for (const auto& k : keys) {
            int col = ds.indexOf(k.variable);
            if (col < 0) continue;
            if (ds.variableAt(static_cast<size_t>(col)).type == VarType::String) {
                const auto& sa = ds.getString(a, static_cast<size_t>(col));
                const auto& sb = ds.getString(b, static_cast<size_t>(col));
                if (sa < sb) return k.ascending;
                if (sa > sb) return !k.ascending;
            } else {
                double va = ds.getNumeric(a, static_cast<size_t>(col));
                double vb = ds.getNumeric(b, static_cast<size_t>(col));
                bool am = isSystemMissing(va), bm = isSystemMissing(vb);
                if (am && bm) continue;
                if (am) return false;
                if (bm) return true;
                if (va < vb) return k.ascending;
                if (va > vb) return !k.ascending;
            }
        }
        return false;
    });
    auto tmp = ds.clone();
    for (size_t c = 0; c < ds.variableCount(); ++c) {
        for (size_t r = 0; r < ds.caseCount(); ++r) {
            size_t src = order[r];
            if (ds.variableAt(c).type == VarType::String)
                ds.setString(r, c, tmp->getString(src, c));
            else
                ds.setNumeric(r, c, tmp->getNumeric(src, c));
        }
    }
}

void TransformationEngine::selectIf(DatasetStore& ds, const std::string& expression, bool filterMode) {
    ds.pushUndo();
    const std::string filterName = "filter_$";
    int idx = ds.indexOf(filterName);
    if (idx < 0) {
        Variable v;
        v.name = filterName;
        v.type = VarType::Numeric;
        v.label = "Filter";
        idx = static_cast<int>(ds.addVariable(std::move(v)));
    }
    for (size_t r = 0; r < ds.caseCount(); ++r) {
        double v = 0;
        try { v = evalExpression(ds, r, expression); }
        catch (...) { v = 0; }
        ds.setNumeric(r, static_cast<size_t>(idx), (!isSystemMissing(v) && v != 0) ? 1.0 : 0.0);
    }
    if (filterMode) {
        ds.flags().filterOn = true;
        ds.flags().filterVariable = filterName;
    } else {
        // delete unselected
        for (size_t r = ds.caseCount(); r-- > 0;) {
            if (ds.getNumeric(r, static_cast<size_t>(idx)) == 0) ds.deleteCase(r);
        }
        ds.flags().filterOn = false;
    }
}

void TransformationEngine::weightBy(DatasetStore& ds, const std::string& variable) {
    ds.pushUndo();
    if (variable.empty()) {
        ds.flags().weightOn = false;
        ds.flags().weightVariable.clear();
        return;
    }
    if (!ds.hasVariable(variable)) throw std::runtime_error("Weight variable not found");
    ds.flags().weightOn = true;
    ds.flags().weightVariable = variable;
}

void TransformationEngine::splitFile(DatasetStore& ds, const std::vector<std::string>& vars, bool /*layered*/) {
    ds.pushUndo();
    if (vars.empty()) {
        ds.flags().splitOn = false;
        ds.flags().splitVariables.clear();
        return;
    }
    for (const auto& v : vars) {
        if (!ds.hasVariable(v)) throw std::runtime_error("Split variable not found: " + v);
    }
    ds.flags().splitOn = true;
    ds.flags().splitVariables = vars;
}

std::unique_ptr<DatasetStore> TransformationEngine::aggregate(const DatasetStore& ds, const AggregateSpec& spec) {
    auto out = std::make_unique<DatasetStore>(ds.name() + "_agg");
    std::vector<int> breakIdx;
    for (const auto& b : spec.breakVars) {
        int i = ds.indexOf(b);
        if (i < 0) throw std::runtime_error("Break var missing");
        breakIdx.push_back(i);
        Variable v = ds.variableAt(static_cast<size_t>(i));
        out->addVariable(v);
    }
    struct AggCol { int src; std::string fn; size_t outCol; };
    std::vector<AggCol> cols;
    for (const auto& a : spec.aggregations) {
        int s = ds.indexOf(a.source);
        if (s < 0) throw std::runtime_error("Agg source missing");
        Variable v;
        v.name = a.target;
        v.type = VarType::Numeric;
        size_t oc = out->addVariable(v);
        cols.push_back({s, a.function, oc});
    }

    auto keyOf = [&](size_t r) {
        std::ostringstream oss;
        for (int bi : breakIdx) {
            if (ds.variableAt(static_cast<size_t>(bi)).type == VarType::String)
                oss << ds.getString(r, static_cast<size_t>(bi)) << "\x1f";
            else
                oss << ds.getNumeric(r, static_cast<size_t>(bi)) << "\x1f";
        }
        return oss.str();
    };

    std::map<std::string, std::vector<size_t>> groups;
    for (size_t r = 0; r < ds.caseCount(); ++r) {
        if (!ds.caseSelected(r)) continue;
        groups[keyOf(r)].push_back(r);
    }
    for (const auto& [k, rows] : groups) {
        size_t outRow = out->addCases(1);
        size_t r0 = rows.front();
        for (size_t bi = 0; bi < breakIdx.size(); ++bi) {
            size_t c = static_cast<size_t>(breakIdx[bi]);
            if (ds.variableAt(c).type == VarType::String)
                out->setString(outRow, bi, ds.getString(r0, c));
            else
                out->setNumeric(outRow, bi, ds.getNumeric(r0, c));
        }
        for (const auto& ac : cols) {
            double sum = 0, n = 0, mn = 0, mx = 0;
            bool first = true;
            for (size_t r : rows) {
                if (ds.isMissing(r, static_cast<size_t>(ac.src))) continue;
                double w = ds.caseWeight(r);
                double v = ds.getNumeric(r, static_cast<size_t>(ac.src));
                sum += v * w;
                n += w;
                if (first || v < mn) mn = v;
                if (first || v > mx) mx = v;
                first = false;
            }
            double result = kSystemMissing;
            if (ac.fn == "N") result = n;
            else if (n > 0) {
                if (ac.fn == "SUM") result = sum;
                else if (ac.fn == "MEAN") result = sum / n;
                else if (ac.fn == "MIN") result = mn;
                else if (ac.fn == "MAX") result = mx;
            }
            out->setNumeric(outRow, ac.outCol, result);
        }
    }
    return out;
}

std::unique_ptr<DatasetStore> TransformationEngine::merge(
    const DatasetStore& a, const DatasetStore& b, const MergeSpec& spec) {
    auto out = a.clone();
    out->setName(a.name() + "_merged");
    if (spec.mode == MergeSpec::Mode::AddCases) {
        size_t start = out->addCases(b.caseCount());
        for (size_t c = 0; c < b.variableCount(); ++c) {
            const auto& bv = b.variableAt(c);
            int oi = out->indexOf(bv.name);
            if (oi < 0) {
                Variable v = bv;
                oi = static_cast<int>(out->addVariable(std::move(v), out->caseCount()));
            }
            for (size_t r = 0; r < b.caseCount(); ++r) {
                if (bv.type == VarType::String)
                    out->setString(start + r, static_cast<size_t>(oi), b.getString(r, c));
                else
                    out->setNumeric(start + r, static_cast<size_t>(oi), b.getNumeric(r, c));
            }
        }
    } else {
        for (size_t c = 0; c < b.variableCount(); ++c) {
            const auto& bv = b.variableAt(c);
            if (out->hasVariable(bv.name)) continue;
            bool isKey = false;
            for (const auto& k : spec.keyVars) if (k == bv.name) isKey = true;
            if (isKey) continue;
            Variable v = bv;
            out->addVariable(std::move(v), out->caseCount());
        }
        // Match by key vars positionally if provided, else by row index
        for (size_t r = 0; r < out->caseCount() && r < b.caseCount(); ++r) {
            for (size_t c = 0; c < b.variableCount(); ++c) {
                const auto& bv = b.variableAt(c);
                int oi = out->indexOf(bv.name);
                if (oi < 0) continue;
                if (bv.type == VarType::String)
                    out->setString(r, static_cast<size_t>(oi), b.getString(r, c));
                else
                    out->setNumeric(r, static_cast<size_t>(oi), b.getNumeric(r, c));
            }
        }
    }
    return out;
}

std::unique_ptr<DatasetStore> TransformationEngine::varsToCases(
    const DatasetStore& ds,
    const std::vector<std::string>& stubVars,
    const std::string& indexName,
    const std::string& valueName) {
    auto out = std::make_unique<DatasetStore>(ds.name() + "_long");
    std::vector<std::string> idVars;
    std::set<std::string> stubs(stubVars.begin(), stubVars.end());
    for (const auto& v : ds.variables()) {
        if (!stubs.count(v.name)) {
            idVars.push_back(v.name);
            out->addVariable(v);
        }
    }
    Variable idx;
    idx.name = indexName;
    idx.type = VarType::Numeric;
    out->addVariable(idx);
    Variable val;
    val.name = valueName;
    val.type = VarType::Numeric;
    out->addVariable(val);
    size_t idxCol = out->variableCount() - 2;
    size_t valCol = out->variableCount() - 1;
    for (size_t r = 0; r < ds.caseCount(); ++r) {
        for (size_t s = 0; s < stubVars.size(); ++s) {
            int sc = ds.indexOf(stubVars[s]);
            if (sc < 0) continue;
            size_t orow = out->addCases(1);
            for (size_t i = 0; i < idVars.size(); ++i) {
                int ic = ds.indexOf(idVars[i]);
                if (ds.variableAt(static_cast<size_t>(ic)).type == VarType::String)
                    out->setString(orow, i, ds.getString(r, static_cast<size_t>(ic)));
                else
                    out->setNumeric(orow, i, ds.getNumeric(r, static_cast<size_t>(ic)));
            }
            out->setNumeric(orow, idxCol, static_cast<double>(s + 1));
            out->setNumeric(orow, valCol, ds.getNumeric(r, static_cast<size_t>(sc)));
        }
    }
    return out;
}

std::unique_ptr<DatasetStore> TransformationEngine::casesToVars(
    const DatasetStore& ds,
    const std::vector<std::string>& idVars,
    const std::string& indexVar,
    const std::vector<std::string>& measureVars) {
    auto out = std::make_unique<DatasetStore>(ds.name() + "_wide");
    for (const auto& id : idVars) {
        int i = ds.indexOf(id);
        if (i < 0) throw std::runtime_error("id var missing");
        out->addVariable(ds.variableAt(static_cast<size_t>(i)));
    }
    int indexCol = ds.indexOf(indexVar);
    if (indexCol < 0) throw std::runtime_error("index var missing");

    std::set<double> levels;
    for (size_t r = 0; r < ds.caseCount(); ++r) {
        if (!ds.isMissing(r, static_cast<size_t>(indexCol)))
            levels.insert(ds.getNumeric(r, static_cast<size_t>(indexCol)));
    }
    std::map<std::pair<std::string, double>, size_t> colMap;
    for (const auto& m : measureVars) {
        for (double lv : levels) {
            Variable v;
            v.name = m + "_" + std::to_string(static_cast<int>(lv));
            v.type = VarType::Numeric;
            size_t c = out->addVariable(v);
            colMap[{m, lv}] = c;
        }
    }

    auto idKey = [&](size_t r) {
        std::ostringstream oss;
        for (const auto& id : idVars) {
            int i = ds.indexOf(id);
            if (ds.variableAt(static_cast<size_t>(i)).type == VarType::String)
                oss << ds.getString(r, static_cast<size_t>(i)) << "\x1f";
            else
                oss << ds.getNumeric(r, static_cast<size_t>(i)) << "\x1f";
        }
        return oss.str();
    };

    std::map<std::string, size_t> rowMap;
    for (size_t r = 0; r < ds.caseCount(); ++r) {
        std::string k = idKey(r);
        size_t orow;
        if (!rowMap.count(k)) {
            orow = out->addCases(1);
            rowMap[k] = orow;
            for (size_t i = 0; i < idVars.size(); ++i) {
                int ic = ds.indexOf(idVars[i]);
                if (ds.variableAt(static_cast<size_t>(ic)).type == VarType::String)
                    out->setString(orow, i, ds.getString(r, static_cast<size_t>(ic)));
                else
                    out->setNumeric(orow, i, ds.getNumeric(r, static_cast<size_t>(ic)));
            }
        } else orow = rowMap[k];
        double lv = ds.getNumeric(r, static_cast<size_t>(indexCol));
        for (const auto& m : measureVars) {
            int mc = ds.indexOf(m);
            if (mc < 0) continue;
            auto it = colMap.find({m, lv});
            if (it != colMap.end())
                out->setNumeric(orow, it->second, ds.getNumeric(r, static_cast<size_t>(mc)));
        }
    }
    return out;
}

} // namespace spss
