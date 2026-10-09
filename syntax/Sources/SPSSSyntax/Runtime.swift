import Foundation
import SPSSCore

public struct SyntaxRunResult: Sendable {
    public var log: [String]
    public var diagnostics: [SyntaxDiagnostic]
    public var outputHTML: [String]
}

@MainActor
public final class SyntaxRuntime {
    public init() {}

    public func run(text: String, session: ProjectSession, selectionOnly: Bool = false, selectedRange: Range<String.Index>? = nil) -> SyntaxRunResult {
        let source: String
        if selectionOnly, let selectedRange {
            source = String(text[selectedRange])
        } else {
            source = text
        }
        var log: [String] = []
        var html: [String] = []
        var diagnostics: [SyntaxDiagnostic] = []

        // Strip multi-line BEGIN PROGRAM … END PROGRAM so SPSS commands inside aren't mis-parsed.
        let programBlocks = ExtensionHost.extractProgramBlocks(from: source)
        let stripped = Self.stripProgramBlocks(from: source)
        let (units, diags) = SyntaxParser.parse(stripped)
        diagnostics.append(contentsOf: diags)

        for block in programBlocks {
            let result = ExtensionHost.run(runtime: block.runtime, code: block.code)
            log.append("> BEGIN PROGRAM-\(block.runtime.rawValue.uppercased())")
            if !result.stdout.isEmpty { log.append(result.stdout) }
            if !result.stderr.isEmpty { log.append(result.stderr) }
            if result.succeeded {
                session.appendOutput(title: "BEGIN PROGRAM", html: "<pre>\(result.stdout)</pre>")
                html.append("<pre>\(result.stdout)</pre>")
                log.append("BEGIN PROGRAM completed")
            } else {
                diagnostics.append(.init(line: 1, message: "BEGIN PROGRAM failed: \(result.stderr)", isError: true))
                log.append("ERROR: BEGIN PROGRAM")
            }
        }

        session.isRunning = true
        defer { session.isRunning = false }

        for unit in units {
            session.commandHistory.append(unit.raw)
            log.append("> " + unit.raw)
            do {
                if session.activeDataset == nil {
                    switch unit.command {
                    case .beginProgram, .execute, .unknown:
                        break
                    default:
                        diagnostics.append(.init(line: unit.line, message: "No active dataset", isError: true))
                        log.append("ERROR: No active dataset")
                        continue
                    }
                }
                let ds = session.activeDataset
                switch unit.command {
                case .compute(let target, let expression):
                    try ds!.engine.compute(target: target, expression: expression)
                    ds!.bump()
                    log.append("COMPUTE completed")
                case .recode(let source, let target, let pairs):
                    for (from, to) in pairs {
                        try ds!.engine.recodeValue(source: source, target: target, from: from, to: to)
                    }
                    ds!.bump()
                    log.append("RECODE completed")
                case .sort(let variable, let ascending):
                    try ds!.engine.sort(variable: variable, ascending: ascending)
                    ds!.bump()
                    log.append("SORT completed")
                case .selectIf(let expression):
                    try ds!.engine.filterSelect(expression: expression)
                    ds!.filterOn = true
                    ds!.bump()
                    log.append("SELECT IF / FILTER applied")
                case .weight(let variable):
                    try ds!.engine.weight(by: variable)
                    ds!.weightOn = variable != nil
                    ds!.bump()
                    log.append(variable == nil ? "WEIGHT OFF" : "WEIGHT BY \(variable!)")
                case .split(let variables):
                    ds!.splitOn = !variables.isEmpty
                    ds!.bump()
                    log.append(variables.isEmpty ? "SPLIT FILE OFF" : "SPLIT FILE BY \(variables.joined(separator: " "))")
                case .frequencies(let variables):
                    let out = ds!.engine.frequencies(variables)
                    html.append(out.html)
                    session.appendOutput(title: "FREQUENCIES", html: out.html)
                    log.append("FREQUENCIES completed")
                case .descriptives(let variables):
                    let out = ds!.engine.descriptives(variables)
                    html.append(out.html)
                    session.appendOutput(title: "DESCRIPTIVES", html: out.html)
                    log.append("DESCRIPTIVES completed")
                case .examine(let variables):
                    let out = ds!.engine.descriptives(variables)
                    html.append(out.html)
                    session.appendOutput(title: "EXAMINE", html: out.html)
                    log.append("EXAMINE completed")
                case .crosstabs(let row, let col):
                    let out = ds!.engine.crosstabs(row: row, col: col)
                    html.append(out.html)
                    session.appendOutput(title: "CROSSTABS", html: out.html)
                    log.append("CROSSTABS completed")
                case .ttest(let variable, let testValue):
                    let out = ds!.engine.ttestOneSample(variable: variable, testValue: testValue)
                    html.append(out.html)
                    session.appendOutput(title: "T-TEST", html: out.html)
                    log.append("T-TEST completed")
                case .oneway(let dependent, let factor):
                    let out = ds!.engine.oneway(dependent: dependent, factor: factor)
                    html.append(out.html)
                    session.appendOutput(title: "ONEWAY", html: out.html)
                    log.append("ONEWAY completed")
                case .correlations(let variables):
                    let out = ds!.engine.correlation(variables)
                    html.append(out.html)
                    session.appendOutput(title: "CORRELATIONS", html: out.html)
                    log.append("CORRELATIONS completed")
                case .regression(let dependent, let independents):
                    let out = ds!.engine.regression(dependent: dependent, independents: independents)
                    html.append(out.html)
                    session.appendOutput(title: "REGRESSION", html: out.html)
                    log.append("REGRESSION completed")
                case .unianova(let dependent, let factors, let covariates):
                    let out = ds!.engine.glm(dependent: dependent, factors: factors, covariates: covariates)
                    html.append(out.html)
                    session.appendOutput(title: "UNIANOVA", html: out.html)
                    log.append("UNIANOVA completed")
                case .factor(let variables, let nFactors):
                    let out = ds!.engine.factor(variables, nFactors: nFactors)
                    html.append(out.html)
                    session.appendOutput(title: "FACTOR", html: out.html)
                    log.append("FACTOR completed")
                case .cluster(let variables, let k):
                    let out = ds!.engine.cluster(variables, k: k)
                    html.append(out.html)
                    session.appendOutput(title: "QUICK CLUSTER", html: out.html)
                    log.append("QUICK CLUSTER completed")
                case .logistic(let dependent, let independents):
                    let out = ds!.engine.logistic(dependent: dependent, independents: independents)
                    html.append(out.html)
                    session.appendOutput(title: "LOGISTIC REGRESSION", html: out.html)
                    log.append("LOGISTIC REGRESSION completed")
                case .km(let time, let status):
                    let out = ds!.engine.kaplanMeier(time: time, status: status)
                    html.append(out.html)
                    session.appendOutput(title: "KM", html: out.html)
                    log.append("KM completed")
                case .impute(let variables, let m):
                    let out = ds!.engine.impute(variables, m: m)
                    html.append(out.html)
                    session.appendOutput(title: "MULTIPLE IMPUTATION", html: out.html)
                    log.append("MULTIPLE IMPUTATION completed")
                case .bootstrap(let variables, let samples):
                    let out = ds!.engine.bootstrap(variables, samples: samples)
                    html.append(out.html)
                    session.appendOutput(title: "BOOTSTRAP", html: out.html)
                    log.append("BOOTSTRAP completed")
                case .forecast(let variable, let periods, let window):
                    let out = ds!.engine.forecast(variable: variable, periods: periods, window: window)
                    html.append(out.html)
                    session.appendOutput(title: "TSMODEL", html: out.html)
                    log.append("TSMODEL completed")
                case .ctables(let row, let column, let summary):
                    let out = ds!.engine.ctables(row: row, column: column, summary: summary)
                    html.append(out.html)
                    session.appendOutput(title: "CTABLES", html: out.html)
                    log.append("CTABLES completed")
                case .tree(let dependent, let predictor):
                    let out = ds!.engine.tree(dependent: dependent, predictor: predictor)
                    html.append(out.html)
                    session.appendOutput(title: "TREE", html: out.html)
                    log.append("TREE completed")
                case .exact(let row, let column):
                    let out = ds!.engine.exact(row: row, column: column)
                    html.append(out.html)
                    session.appendOutput(title: "EXACT", html: out.html)
                    log.append("EXACT completed")
                case .neural(let dependent, let independents):
                    let out = ds!.engine.neural(dependent: dependent, independents: independents)
                    html.append(out.html)
                    session.appendOutput(title: "NEURAL NETWORK", html: out.html)
                    log.append("NEURAL NETWORK completed")
                case .beginProgram(let runtimeName, let code):
                    if code.isEmpty {
                        log.append("BEGIN PROGRAM (see multi-line block handler)")
                    } else {
                        let runtime: ExtensionRuntime = runtimeName.uppercased().contains("R") ? .r : .python
                        let result = ExtensionHost.run(runtime: runtime, code: code)
                        if !result.stdout.isEmpty { log.append(result.stdout); html.append("<pre>\(result.stdout)</pre>") }
                        if !result.succeeded {
                            diagnostics.append(.init(line: unit.line, message: result.stderr, isError: true))
                        } else {
                            session.appendOutput(title: "BEGIN PROGRAM", html: "<pre>\(result.stdout)</pre>")
                            log.append("BEGIN PROGRAM completed")
                        }
                    }
                case .execute:
                    log.append("EXECUTE")
                case .unknown(let name, _):
                    diagnostics.append(.init(line: unit.line, message: "Did not execute unknown/unsupported command: \(name)", isError: true))
                    log.append("ERROR: \(name)")
                }
            } catch {
                diagnostics.append(.init(line: unit.line, message: error.localizedDescription, isError: true))
                log.append("ERROR: \(error.localizedDescription)")
            }
        }
        session.statusMessage = diagnostics.contains(where: \.isError) ? "Completed with errors" : "Syntax run complete"
        return SyntaxRunResult(log: log, diagnostics: diagnostics, outputHTML: html)
    }

    private static func stripProgramBlocks(from text: String) -> String {
        var out: [String] = []
        var inBlock = false
        for line in text.components(separatedBy: .newlines) {
            let u = line.trimmingCharacters(in: .whitespacesAndNewlines).uppercased()
            if u.hasPrefix("BEGIN PROGRAM") {
                inBlock = true
                continue
            }
            if u.hasPrefix("END PROGRAM") {
                inBlock = false
                continue
            }
            if !inBlock { out.append(line) }
        }
        return out.joined(separator: "\n")
    }
}
