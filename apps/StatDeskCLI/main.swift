import Foundation
import SPSSCore
import SPSSSyntax

@main
enum StatDeskCLIEntry {
    static func main() {
        let args = CommandLine.arguments
        guard args.count >= 2 else {
            print("""
            statdesk-cli — StatDesk headless runner
              descriptives <csv> <var>...
              frequencies <csv> <var>...
              glm <csv> <dep> <factor>
              factor <csv> <var>...
              cluster <csv> <k> <var>...
              logistic <csv> <dep> <indep>...
              ctables <csv> <row> <col>
              syntax <csv> <file.sps>
              extensions
            """)
            return
        }
        let cmd = args[1]
        do {
            switch cmd {
            case "descriptives":
                let ds = try EngineDataset.readCSV(path: args[2])
                print(ds.descriptives(Array(args.dropFirst(3))).html)
            case "frequencies":
                let ds = try EngineDataset.readCSV(path: args[2])
                print(ds.frequencies(Array(args.dropFirst(3))).html)
            case "glm":
                let ds = try EngineDataset.readCSV(path: args[2])
                print(ds.glm(dependent: args[3], factors: [args[4]]).html)
            case "factor":
                let ds = try EngineDataset.readCSV(path: args[2])
                print(ds.factor(Array(args.dropFirst(3))).html)
            case "cluster":
                let ds = try EngineDataset.readCSV(path: args[2])
                let k = Int(args[3]) ?? 2
                print(ds.cluster(Array(args.dropFirst(4)), k: k).html)
            case "logistic":
                let ds = try EngineDataset.readCSV(path: args[2])
                print(ds.logistic(dependent: args[3], independents: Array(args.dropFirst(4))).html)
            case "ctables":
                let ds = try EngineDataset.readCSV(path: args[2])
                print(ds.ctables(row: args[3], column: args[4]).html)
            case "extensions":
                for cap in ExtensionHost.status() {
                    print("\(cap.runtime.rawValue): \(cap.available ? "OK \(cap.interpreterPath ?? "")" : cap.reason)")
                }
            case "syntax":
                try runSyntaxHeadless(csv: args[2], sps: args[3])
            default:
                fputs("Unknown command\n", stderr)
                exit(1)
            }
        } catch {
            fputs("Error: \(error.localizedDescription)\n", stderr)
            exit(1)
        }
    }

    /// Headless syntax path — no MainActor UI session (avoids CLT concurrency deadlocks).
    private static func runSyntaxHeadless(csv: String, sps: String) throws {
        let ds = try EngineDataset.readCSV(path: csv)
        let text = try String(contentsOfFile: sps, encoding: .utf8)
        for block in ExtensionHost.extractProgramBlocks(from: text) {
            print("> BEGIN PROGRAM-\(block.runtime.rawValue)")
            let result = ExtensionHost.run(runtime: block.runtime, code: block.code)
            if !result.stdout.isEmpty { print(result.stdout) }
            if !result.stderr.isEmpty { fputs(result.stderr + "\n", stderr) }
        }
        let stripped = stripProgramBlocks(text)
        let (units, diags) = SyntaxParser.parse(stripped)
        for d in diags where d.isError {
            fputs("ERROR line \(d.line): \(d.message)\n", stderr)
        }
        for unit in units {
            print("> \(unit.raw)")
            switch unit.command {
            case .compute(let target, let expression):
                try ds.compute(target: target, expression: expression)
                print("COMPUTE completed")
            case .recode(let source, let target, let pairs):
                for (from, to) in pairs {
                    try ds.recodeValue(source: source, target: target, from: from, to: to)
                }
                print("RECODE completed")
            case .sort(let variable, let ascending):
                try ds.sort(variable: variable, ascending: ascending)
                print("SORT completed")
            case .selectIf(let expression):
                try ds.filterSelect(expression: expression)
                print("FILTER applied")
            case .weight(let variable):
                try ds.weight(by: variable)
                print("WEIGHT updated")
            case .split:
                print("SPLIT noted (flags UI-only in headless)")
            case .frequencies(let variables):
                print(ds.frequencies(variables).html)
            case .descriptives(let variables), .examine(let variables):
                print(ds.descriptives(variables).html)
            case .crosstabs(let row, let col):
                print(ds.crosstabs(row: row, col: col).html)
            case .ttest(let variable, let testValue):
                print(ds.ttestOneSample(variable: variable, testValue: testValue).html)
            case .oneway(let dependent, let factor):
                print(ds.oneway(dependent: dependent, factor: factor).html)
            case .correlations(let variables):
                print(ds.correlation(variables).html)
            case .regression(let dependent, let independents):
                print(ds.regression(dependent: dependent, independents: independents).html)
            case .unianova(let dependent, let factors, let covariates):
                print(ds.glm(dependent: dependent, factors: factors, covariates: covariates).html)
            case .factor(let variables, let nFactors):
                print(ds.factor(variables, nFactors: nFactors).html)
            case .cluster(let variables, let k):
                print(ds.cluster(variables, k: k).html)
            case .logistic(let dependent, let independents):
                print(ds.logistic(dependent: dependent, independents: independents).html)
            case .km(let time, let status):
                print(ds.kaplanMeier(time: time, status: status).html)
            case .impute(let variables, let m):
                print(ds.impute(variables, m: m).html)
            case .bootstrap(let variables, let samples):
                print(ds.bootstrap(variables, samples: samples).html)
            case .forecast(let variable, let periods, let window):
                print(ds.forecast(variable: variable, periods: periods, window: window).html)
            case .ctables(let row, let column, let summary):
                print(ds.ctables(row: row, column: column, summary: summary).html)
            case .tree(let dependent, let predictor):
                print(ds.tree(dependent: dependent, predictor: predictor).html)
            case .exact(let row, let column):
                print(ds.exact(row: row, column: column).html)
            case .neural(let dependent, let independents):
                print(ds.neural(dependent: dependent, independents: independents).html)
            case .beginProgram(let runtimeName, let code):
                if !code.isEmpty {
                    let runtime: ExtensionRuntime = runtimeName.uppercased().contains("R") ? .r : .python
                    let result = ExtensionHost.run(runtime: runtime, code: code)
                    print(result.stdout)
                    if !result.succeeded { fputs(result.stderr + "\n", stderr) }
                }
            case .execute:
                print("EXECUTE")
            case .unknown(let name, _):
                fputs("ERROR: unsupported \(name)\n", stderr)
            }
        }
    }

    private static func stripProgramBlocks(_ text: String) -> String {
        var out: [String] = []
        var inBlock = false
        for line in text.components(separatedBy: .newlines) {
            let u = line.trimmingCharacters(in: .whitespacesAndNewlines).uppercased()
            if u.hasPrefix("BEGIN PROGRAM") { inBlock = true; continue }
            if u.hasPrefix("END PROGRAM") { inBlock = false; continue }
            if !inBlock { out.append(line) }
        }
        return out.joined(separator: "\n")
    }
}
