import Foundation

public struct SyntaxDiagnostic: Identifiable, Sendable {
    public let id = UUID()
    public var line: Int
    public var message: String
    public var isError: Bool
}

public enum ParsedCommand: Sendable {
    case compute(target: String, expression: String)
    case recode(source: String, target: String, pairs: [(Double, Double)])
    case sort(variable: String, ascending: Bool)
    case selectIf(expression: String)
    case weight(variable: String?)
    case split(variables: [String])
    case frequencies(variables: [String])
    case descriptives(variables: [String])
    case examine(variables: [String])
    case crosstabs(row: String, col: String)
    case ttest(variable: String, testValue: Double)
    case oneway(dependent: String, factor: String)
    case correlations(variables: [String])
    case regression(dependent: String, independents: [String])
    case unianova(dependent: String, factors: [String], covariates: [String])
    case factor(variables: [String], nFactors: Int)
    case cluster(variables: [String], k: Int)
    case logistic(dependent: String, independents: [String])
    case km(time: String, status: String)
    case impute(variables: [String], m: Int)
    case bootstrap(variables: [String], samples: Int)
    case forecast(variable: String, periods: Int, window: Int)
    case ctables(row: String, column: String, summary: String?)
    case tree(dependent: String, predictor: String)
    case exact(row: String, column: String)
    case neural(dependent: String, independents: [String])
    case beginProgram(runtime: String, code: String)
    case execute
    case unknown(name: String, raw: String)
}

public struct ParsedUnit: Sendable {
    public var line: Int
    public var raw: String
    public var command: ParsedCommand
}

public enum SyntaxParser {
    /// Split on periods that terminate commands (SPSS style), ignoring decimals inside numbers roughly.
    public static func parse(_ text: String) -> (units: [ParsedUnit], diagnostics: [SyntaxDiagnostic]) {
        var diagnostics: [SyntaxDiagnostic] = []
        var units: [ParsedUnit] = []
        let lines = text.components(separatedBy: .newlines)
        var buffer = ""
        var startLine = 1
        for (idx, line) in lines.enumerated() {
            let lineNo = idx + 1
            let trimmed = stripComment(line)
            if trimmed.isEmpty { continue }
            if buffer.isEmpty { startLine = lineNo }
            buffer += (buffer.isEmpty ? "" : " ") + trimmed
            if trimmed.hasSuffix(".") || buffer.hasSuffix(".") {
                let cmdText = String(buffer.dropLast().trimmingCharacters(in: .whitespaces))
                buffer = ""
                if cmdText.isEmpty { continue }
                let (cmd, diag) = parseCommand(cmdText, line: startLine)
                if let diag { diagnostics.append(diag) }
                units.append(ParsedUnit(line: startLine, raw: cmdText, command: cmd))
            }
        }
        if !buffer.trimmingCharacters(in: .whitespaces).isEmpty {
            diagnostics.append(.init(line: startLine, message: "Command not terminated with '.'", isError: true))
        }
        return (units, diagnostics)
    }

    private static func stripComment(_ line: String) -> String {
        let s = line
        if let r = s.range(of: "*") {
            // SPSS block comments are harder; treat * at start as full-line comment
            let prefix = s[..<r.lowerBound].trimmingCharacters(in: .whitespaces)
            if prefix.isEmpty { return "" }
        }
        return s.trimmingCharacters(in: .whitespacesAndNewlines)
    }

    private static func parseCommand(_ text: String, line: Int) -> (ParsedCommand, SyntaxDiagnostic?) {
        let upper = text.uppercased()
        let tokens = tokenize(text)

        func varsAfter(_ keyword: String) -> [String] {
            let key = keyword.uppercased()
            // Support "/VARIABLES=a b", "VARIABLES=a b", and "VARIABLES a b"
            if let i = tokens.firstIndex(where: {
                let u = $0.uppercased().trimmingCharacters(in: CharacterSet(charactersIn: "/"))
                return u.hasPrefix(key + "=") || u == key
            }) {
                let raw = tokens[i]
                let u = raw.uppercased().trimmingCharacters(in: CharacterSet(charactersIn: "/"))
                var out: [String] = []
                if u.hasPrefix(key + "=") {
                    let first = cleanToken(raw)
                    if !first.isEmpty { out.append(first) }
                    out.append(contentsOf: tokens.suffix(from: i + 1).map(cleanToken).filter(isVarToken))
                } else {
                    out.append(contentsOf: tokens.suffix(from: i + 1).map(cleanToken).filter(isVarToken))
                }
                return out
            }
            return []
        }

        func cleanToken(_ t: String) -> String {
            var s = t
            while s.hasPrefix("/") { s = String(s.dropFirst()) }
            if let eq = s.firstIndex(of: "=") {
                s = String(s[s.index(after: eq)...])
            }
            return s.trimmingCharacters(in: CharacterSet(charactersIn: "()"))
        }

        func isVarToken(_ t: String) -> Bool {
            let u = t.uppercased()
            if t.isEmpty || t == "/" { return false }
            let stop: Set<String> = [
                "BY", "INTO", "ORDER", "STATISTICS", "PRINT", "METHOD", "DEPENDENT",
                "TESTVAL", "PLOT", "TABLES", "VARIABLES", "ANALYSIS", "TWOTAIL",
                "NOSIG", "CHISQ", "MEAN", "STDDEV", "MIN", "MAX", "BOXPLOT", "HISTOGRAM",
                "ENTER", "A", "D", "CRITERIA", "EXTRACTION", "PC", "WITH", "STATUS",
                "NIMPUTATIONS", "SAMPLES", "PERIODS", "FACTORS", "CLUSTER"
            ]
            if stop.contains(u) { return false }
            if u.contains("=") { return false }
            if u.contains("(") { return false }
            return true
        }

        if upper.hasPrefix("COMPUTE ") {
            // COMPUTE x = expr
            if let eq = text.firstIndex(of: "=") {
                let left = text[text.index(text.startIndex, offsetBy: 8)..<eq].trimmingCharacters(in: .whitespaces)
                let right = text[text.index(after: eq)...].trimmingCharacters(in: .whitespaces)
                return (.compute(target: left, expression: String(right)), nil)
            }
            return (.unknown(name: "COMPUTE", raw: text), .init(line: line, message: "COMPUTE requires target = expression", isError: true))
        }

        if upper.hasPrefix("RECODE ") {
            // RECODE src (1=2) (3=4) INTO tgt   or simple RECODE src (1=2).
            let parts = tokens
            guard parts.count >= 2 else {
                return (.unknown(name: "RECODE", raw: text), .init(line: line, message: "RECODE syntax error", isError: true))
            }
            let source = parts[1]
            var target = source
            if let into = parts.firstIndex(where: { $0.uppercased() == "INTO" }), into + 1 < parts.count {
                target = parts[into + 1]
            }
            var pairs: [(Double, Double)] = []
            let pattern = #/\((\-?\d+(?:\.\d+)?)\s*=\s*(\-?\d+(?:\.\d+)?)\)/#
            for match in text.matches(of: pattern) {
                if let a = Double(match.1), let b = Double(match.2) {
                    pairs.append((a, b))
                }
            }
            return (.recode(source: source, target: target, pairs: pairs), nil)
        }

        if upper.hasPrefix("SORT CASES") || upper.hasPrefix("SORT ") {
            let by = varsAfter("BY")
            let v = by.first ?? tokens.dropFirst().first(where: { !$0.hasPrefix("/") && $0.uppercased() != "CASES" }) ?? "V1"
            let asc = !upper.contains("D)") && !upper.contains("(D)")
            return (.sort(variable: v, ascending: asc), nil)
        }

        if upper.hasPrefix("SELECT IF") {
            let expr = text.replacingOccurrences(of: "SELECT IF", with: "", options: .caseInsensitive)
                .trimmingCharacters(in: .whitespaces)
            return (.selectIf(expression: expr), nil)
        }

        if upper.hasPrefix("WEIGHT BY") {
            let v = tokens.last ?? ""
            return (.weight(variable: v.uppercased() == "OFF" ? nil : v), nil)
        }
        if upper.hasPrefix("WEIGHT OFF") {
            return (.weight(variable: nil), nil)
        }

        if upper.hasPrefix("SPLIT FILE") {
            if upper.contains("OFF") { return (.split(variables: []), nil) }
            let vars = varsAfter("BY")
            return (.split(variables: vars), nil)
        }

        if upper.hasPrefix("FREQUENCIES") {
            let vars = varsAfter("VARIABLES")
            return (.frequencies(variables: vars.isEmpty ? Array(tokens.dropFirst()) : vars), nil)
        }

        if upper.hasPrefix("DESCRIPTIVES") {
            let vars = varsAfter("VARIABLES")
            return (.descriptives(variables: vars.isEmpty ? Array(tokens.dropFirst()) : vars), nil)
        }

        if upper.hasPrefix("EXAMINE") {
            let vars = varsAfter("VARIABLES")
            return (.examine(variables: vars.isEmpty ? Array(tokens.dropFirst()) : vars), nil)
        }

        if upper.hasPrefix("CROSSTABS") {
            // CROSSTABS /TABLES=row BY col
            let byIdx = tokens.firstIndex(where: { $0.uppercased() == "BY" })
            if let byIdx, byIdx > 0, byIdx + 1 < tokens.count {
                let row = cleanToken(tokens[byIdx - 1])
                let col = cleanToken(tokens[byIdx + 1])
                return (.crosstabs(row: row, col: col), nil)
            }
            return (.unknown(name: "CROSSTABS", raw: text), .init(line: line, message: "CROSSTABS expects /TABLES=row BY col", isError: true))
        }

        if upper.hasPrefix("T-TEST") || upper.hasPrefix("TTEST") {
            let vars = varsAfter("VARIABLES")
            var testValue = 0.0
            if let range = upper.range(of: "TESTVAL=") {
                let rest = text[range.upperBound...]
                testValue = Double(rest.prefix(while: { $0.isNumber || $0 == "." || $0 == "-" })) ?? 0
            }
            let v = vars.first(where: { $0.uppercased() != "TESTVAL" }) ?? "V1"
            return (.ttest(variable: v, testValue: testValue), nil)
        }

        if upper.hasPrefix("ONEWAY") {
            // ONEWAY score BY group
            if let by = tokens.firstIndex(where: { $0.uppercased() == "BY" }), by > 0, by + 1 < tokens.count {
                return (.oneway(dependent: tokens[by - 1], factor: tokens[by + 1]), nil)
            }
            return (.unknown(name: "ONEWAY", raw: text), .init(line: line, message: "ONEWAY expects dep BY factor", isError: true))
        }

        if upper.hasPrefix("CORRELATIONS") {
            let vars = varsAfter("VARIABLES")
            return (.correlations(variables: vars.isEmpty ? Array(tokens.dropFirst()) : vars), nil)
        }

        if upper.hasPrefix("REGRESSION") {
            // REGRESSION /DEPENDENT y /METHOD=ENTER x1 x2
            var dep = ""
            var indep: [String] = []
            if let d = tokens.firstIndex(where: { $0.uppercased().hasPrefix("DEPENDENT") }) {
                if tokens[d].contains("=") {
                    dep = tokens[d].split(separator: "=").last.map(String.init) ?? ""
                } else if d + 1 < tokens.count {
                    dep = tokens[d + 1]
                }
            }
            if let m = tokens.firstIndex(where: { $0.uppercased().hasPrefix("ENTER") || $0.uppercased() == "/METHOD=ENTER" }) {
                indep = Array(tokens.suffix(from: m + 1)).filter { !$0.hasPrefix("/") }
            }
            if dep.isEmpty {
                return (.unknown(name: "REGRESSION", raw: text), .init(line: line, message: "REGRESSION needs /DEPENDENT", isError: true))
            }
            return (.regression(dependent: dep, independents: indep), nil)
        }

        if upper == "EXECUTE" || upper == "EXE" {
            return (.execute, nil)
        }

        if upper.hasPrefix("UNIANOVA") || upper.hasPrefix("GLM ") {
            // UNIANOVA score BY group WITH age
            var dep = ""
            var factors: [String] = []
            var covs: [String] = []
            if let by = tokens.firstIndex(where: { $0.uppercased() == "BY" }), by > 0 {
                dep = cleanToken(tokens[by - 1])
                let afterBy = Array(tokens.suffix(from: by + 1))
                if let with = afterBy.firstIndex(where: { $0.uppercased() == "WITH" }) {
                    factors = afterBy[..<with].map(cleanToken).filter(isVarToken)
                    covs = Array(afterBy.suffix(from: with + 1)).map(cleanToken).filter(isVarToken)
                } else {
                    factors = afterBy.map(cleanToken).filter(isVarToken)
                }
            } else {
                let vars = varsAfter("VARIABLES")
                dep = vars.first ?? ""
            }
            if dep.isEmpty {
                return (.unknown(name: "UNIANOVA", raw: text), .init(line: line, message: "UNIANOVA expects dep BY factor…", isError: true))
            }
            return (.unianova(dependent: dep, factors: factors, covariates: covs), nil)
        }

        if upper.hasPrefix("FACTOR") {
            let vars = varsAfter("VARIABLES")
            var n = 0
            if let range = upper.range(of: "CRITERIA=FACTORS(") {
                let rest = text[range.upperBound...]
                n = Int(rest.prefix(while: { $0.isNumber })) ?? 0
            }
            return (.factor(variables: vars.isEmpty ? Array(tokens.dropFirst()).map(cleanToken).filter(isVarToken) : vars, nFactors: n), nil)
        }

        if upper.hasPrefix("QUICK CLUSTER") || upper.hasPrefix("CLUSTER") {
            let vars = varsAfter("VARIABLES")
            var k = 2
            if let range = upper.range(of: "/CRITERIA=CLUSTER(") ?? upper.range(of: "CLUSTER(") {
                let rest = text[range.upperBound...]
                k = Int(rest.prefix(while: { $0.isNumber })) ?? 2
            }
            return (.cluster(variables: vars.isEmpty ? Array(tokens.dropFirst(upper.hasPrefix("QUICK") ? 2 : 1)).map(cleanToken).filter(isVarToken) : vars, k: k), nil)
        }

        if upper.hasPrefix("LOGISTIC REGRESSION") || upper.hasPrefix("LOGISTIC") {
            var dep = ""
            var indep: [String] = []
            let depVars = varsAfter("VARIABLES")
            if !depVars.isEmpty {
                dep = depVars[0]
            } else if let d = tokens.firstIndex(where: {
                let u = $0.uppercased().trimmingCharacters(in: CharacterSet(charactersIn: "/"))
                return u.hasPrefix("DEPENDENT")
            }) {
                dep = cleanToken(tokens[d])
                if dep.isEmpty && d + 1 < tokens.count { dep = cleanToken(tokens[d + 1]) }
            }
            if let with = tokens.firstIndex(where: {
                let u = $0.uppercased().trimmingCharacters(in: CharacterSet(charactersIn: "/"))
                return u == "WITH" || u.hasPrefix("METHOD") || u == "ENTER" || u.hasPrefix("ENTER")
            }) {
                indep.append(contentsOf: Array(tokens.suffix(from: with + 1)).map(cleanToken).filter(isVarToken))
            }
            if dep.isEmpty {
                return (.unknown(name: "LOGISTIC REGRESSION", raw: text), .init(line: line, message: "LOGISTIC needs dependent", isError: true))
            }
            return (.logistic(dependent: dep, independents: indep), nil)
        }

        if upper.hasPrefix("KM ") || upper.hasPrefix("KAPLAN") {
            // KM time STATUS status
            let vars = tokens.dropFirst().map(cleanToken).filter(isVarToken)
            let time = vars.first ?? ""
            var status = ""
            if let s = tokens.firstIndex(where: { $0.uppercased() == "STATUS" || $0.uppercased().hasPrefix("STATUS=") }) {
                if tokens[s].contains("=") {
                    status = String(tokens[s].split(separator: "=").last ?? "")
                } else if s + 1 < tokens.count {
                    status = tokens[s + 1].split(separator: "(").first.map(String.init) ?? tokens[s + 1]
                }
            }
            if time.isEmpty || status.isEmpty {
                return (.unknown(name: "KM", raw: text), .init(line: line, message: "KM expects time STATUS statusvar", isError: true))
            }
            return (.km(time: time, status: status), nil)
        }

        if upper.hasPrefix("MULTIPLE IMPUTATION") || upper.hasPrefix("MULTIPLE") {
            let vars = varsAfter("VARIABLES")
            var m = 5
            if let range = upper.range(of: "NIMPUTATIONS=") {
                let rest = text[range.upperBound...]
                m = Int(rest.prefix(while: { $0.isNumber })) ?? 5
            }
            return (.impute(variables: vars, m: m), nil)
        }

        if upper.hasPrefix("BOOTSTRAP") {
            let vars = varsAfter("VARIABLES")
            var samples = 200
            if let range = upper.range(of: "SAMPLES=") {
                let rest = text[range.upperBound...]
                samples = Int(rest.prefix(while: { $0.isNumber })) ?? 200
            }
            return (.bootstrap(variables: vars.isEmpty ? Array(tokens.dropFirst()).map(cleanToken).filter(isVarToken) : vars, samples: samples), nil)
        }

        if upper.hasPrefix("TSMODEL") {
            let vars = varsAfter("VARIABLES")
            let v = vars.first ?? tokens.dropFirst().first.map(cleanToken) ?? ""
            var periods = 3
            if let range = upper.range(of: "PERIODS=") {
                let rest = text[range.upperBound...]
                periods = Int(rest.prefix(while: { $0.isNumber })) ?? 3
            }
            return (.forecast(variable: v, periods: periods, window: 3), nil)
        }

        if upper.hasPrefix("CTABLES") {
            // CTABLES /TABLE row BY col
            if let by = tokens.firstIndex(where: { $0.uppercased() == "BY" }), by > 0, by + 1 < tokens.count {
                let row = cleanToken(tokens[by - 1])
                let col = cleanToken(tokens[by + 1])
                return (.ctables(row: row, column: col, summary: nil), nil)
            }
            return (.unknown(name: "CTABLES", raw: text), .init(line: line, message: "CTABLES expects /TABLE row BY col", isError: true))
        }

        if upper.hasPrefix("TREE") {
            // TREE dep BY pred
            if let by = tokens.firstIndex(where: { $0.uppercased() == "BY" }), by > 0, by + 1 < tokens.count {
                return (.tree(dependent: cleanToken(tokens[by - 1]), predictor: cleanToken(tokens[by + 1])), nil)
            }
            return (.unknown(name: "TREE", raw: text), .init(line: line, message: "TREE expects dep BY predictor", isError: true))
        }

        if upper.hasPrefix("EXACT") {
            if let by = tokens.firstIndex(where: { $0.uppercased() == "BY" }), by > 0, by + 1 < tokens.count {
                return (.exact(row: cleanToken(tokens[by - 1]), column: cleanToken(tokens[by + 1])), nil)
            }
            // EXACT TABLES=row BY col  — reuse crosstabs-like tokens
            let vars = varsAfter("TABLES")
            if vars.count >= 2 {
                return (.exact(row: vars[0], column: vars[1]), nil)
            }
            return (.unknown(name: "EXACT", raw: text), .init(line: line, message: "EXACT expects row BY col", isError: true))
        }

        if upper.hasPrefix("NEURAL") {
            var dep = ""
            var indep: [String] = []
            if let d = tokens.firstIndex(where: { $0.uppercased().hasPrefix("DEPENDENT") }) {
                dep = tokens[d].contains("=")
                    ? String(tokens[d].split(separator: "=").last ?? "")
                    : (d + 1 < tokens.count ? tokens[d + 1] : "")
            }
            if let f = tokens.firstIndex(where: { $0.uppercased().hasPrefix("FACTORS") || $0.uppercased() == "WITH" || $0.uppercased() == "ENTER" }) {
                indep = Array(tokens.suffix(from: f + (tokens[f].contains("=") ? 0 : 1))).map(cleanToken).filter(isVarToken)
                if tokens[f].contains("=") {
                    let first = String(tokens[f].split(separator: "=").last ?? "")
                    if !first.isEmpty { indep.insert(first, at: 0) }
                }
            }
            if dep.isEmpty {
                return (.unknown(name: "NEURAL NETWORK", raw: text), .init(line: line, message: "NEURAL needs /DEPENDENT", isError: true))
            }
            return (.neural(dependent: dep, independents: indep), nil)
        }

        if upper.hasPrefix("BEGIN PROGRAM") {
            let runtime = upper.contains(" R") || upper.hasSuffix(" R") ? "R" : "Python"
            // Inline one-liner after BEGIN PROGRAM PYTHON: code — multi-line handled by ExtensionHost in runtime
            let parts = text.split(separator: ":", maxSplits: 1).map(String.init)
            let code = parts.count > 1 ? parts[1].trimmingCharacters(in: .whitespaces) : ""
            return (.beginProgram(runtime: runtime, code: code), nil)
        }

        let name = tokens.first?.uppercased() ?? "?"
        if CommandRegistry.lookup(name) == nil {
            return (.unknown(name: name, raw: text),
                    .init(line: line, message: "Unknown command '\(name)'. No silent drop — command not in registry.", isError: true))
        }
        return (.unknown(name: name, raw: text),
                .init(line: line, message: "Command '\(name)' is recognized but not fully parsed yet.", isError: true))
    }

    private static func tokenize(_ text: String) -> [String] {
        text.split{ $0.isWhitespace }.map(String.init)
    }
}
