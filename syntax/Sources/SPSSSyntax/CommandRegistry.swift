import Foundation

public struct SyntaxCommandInfo: Sendable {
    public var name: String
    public var aliases: [String]
    public var module: String
    public var implemented: Bool
}

public enum CommandRegistry {
    public static let baseCommands: [SyntaxCommandInfo] = [
        .init(name: "GET", aliases: ["GET FILE", "GET DATA"], module: "Base", implemented: true),
        .init(name: "SAVE", aliases: ["SAVE OUTFILE"], module: "Base", implemented: true),
        .init(name: "COMPUTE", aliases: [], module: "Base", implemented: true),
        .init(name: "RECODE", aliases: [], module: "Base", implemented: true),
        .init(name: "AUTORECODE", aliases: [], module: "Base", implemented: true),
        .init(name: "RANK", aliases: [], module: "Base", implemented: true),
        .init(name: "SORT", aliases: ["SORT CASES"], module: "Base", implemented: true),
        .init(name: "SELECT", aliases: ["SELECT IF"], module: "Base", implemented: true),
        .init(name: "FILTER", aliases: ["FILTER BY"], module: "Base", implemented: true),
        .init(name: "WEIGHT", aliases: ["WEIGHT BY"], module: "Base", implemented: true),
        .init(name: "SPLIT", aliases: ["SPLIT FILE"], module: "Base", implemented: true),
        .init(name: "FREQUENCIES", aliases: [], module: "Base", implemented: true),
        .init(name: "DESCRIPTIVES", aliases: [], module: "Base", implemented: true),
        .init(name: "EXAMINE", aliases: [], module: "Base", implemented: true),
        .init(name: "CROSSTABS", aliases: [], module: "Base", implemented: true),
        .init(name: "T-TEST", aliases: ["TTEST"], module: "Base", implemented: true),
        .init(name: "ONEWAY", aliases: [], module: "Base", implemented: true),
        .init(name: "CORRELATIONS", aliases: [], module: "Base", implemented: true),
        .init(name: "REGRESSION", aliases: [], module: "Base", implemented: true),
        .init(name: "NPAR", aliases: ["NPAR TESTS"], module: "Base", implemented: true),
        .init(name: "EXECUTE", aliases: ["EXE"], module: "Base", implemented: true),
        .init(name: "DATASET", aliases: ["DATASET NAME", "DATASET ACTIVATE"], module: "Base", implemented: true),
        .init(name: "UNIANOVA", aliases: ["GLM"], module: "Advanced", implemented: true),
        .init(name: "FACTOR", aliases: [], module: "Advanced", implemented: true),
        .init(name: "QUICK", aliases: ["QUICK CLUSTER", "CLUSTER"], module: "Advanced", implemented: true),
        .init(name: "LOGISTIC", aliases: ["LOGISTIC REGRESSION"], module: "Regression", implemented: true),
        .init(name: "KM", aliases: ["KAPLAN-MEIER"], module: "Advanced", implemented: true),
        .init(name: "MULTIPLE", aliases: ["MULTIPLE IMPUTATION"], module: "Missing Values", implemented: true),
        .init(name: "BOOTSTRAP", aliases: [], module: "Base", implemented: true),
        .init(name: "TSMODEL", aliases: [], module: "Forecasting", implemented: true),
        .init(name: "CTABLES", aliases: [], module: "Custom Tables", implemented: true),
        .init(name: "TREE", aliases: [], module: "Decision Trees", implemented: true),
        .init(name: "EXACT", aliases: [], module: "Exact Tests", implemented: true),
        .init(name: "NEURAL", aliases: ["NEURAL NETWORK"], module: "Neural Networks", implemented: true),
        .init(name: "BEGIN", aliases: ["BEGIN PROGRAM"], module: "Base", implemented: true)
    ]

    public static func lookup(_ token: String) -> SyntaxCommandInfo? {
        let u = token.uppercased()
        return baseCommands.first { info in
            info.name == u || info.aliases.contains(where: { $0 == u || $0.hasPrefix(u) })
        }
    }
}
