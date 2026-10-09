import Foundation

public enum ModuleEdition: String, CaseIterable, Sendable {
    case base = "Base"
    case advanced = "Advanced Statistics"
    case regression = "Regression"
    case customTables = "Custom Tables"
    case exactTests = "Exact Tests"
    case complexSamples = "Complex Samples"
    case decisionTrees = "Decision Trees"
    case neuralNets = "Neural Networks"
    case categories = "Categories"
    case conjoint = "Conjoint"
    case directMarketing = "Direct Marketing"
    case missingValues = "Missing Values"
    case forecasting = "Forecasting"
}

public struct Capability: Identifiable, Sendable {
    public var id: String { command }
    public var command: String
    public var menuPath: String
    public var module: ModuleEdition
    public var available: Bool
    public var phase: Int
}

/// Gates Analyze/Transform menus like SPSS editions — implemented modules unlock; others stay dimmed.
@MainActor
public final class CapabilityRegistry: ObservableObject {
    /// Modules with working MVP procedures are enabled by default.
    @Published public var enabledModules: Set<ModuleEdition> = [
        .base, .advanced, .regression, .customTables, .exactTests,
        .decisionTrees, .neuralNets, .missingValues, .forecasting
    ]

    public let all: [Capability]

    public init() {
        all = [
            .init(command: "FREQUENCIES", menuPath: "Analyze > Descriptive Statistics > Frequencies", module: .base, available: true, phase: 4),
            .init(command: "DESCRIPTIVES", menuPath: "Analyze > Descriptive Statistics > Descriptives", module: .base, available: true, phase: 4),
            .init(command: "EXAMINE", menuPath: "Analyze > Descriptive Statistics > Explore", module: .base, available: true, phase: 4),
            .init(command: "CROSSTABS", menuPath: "Analyze > Descriptive Statistics > Crosstabs", module: .base, available: true, phase: 4),
            .init(command: "T-TEST", menuPath: "Analyze > Compare Means > One-Sample T Test", module: .base, available: true, phase: 4),
            .init(command: "ONEWAY", menuPath: "Analyze > Compare Means > One-Way ANOVA", module: .base, available: true, phase: 4),
            .init(command: "CORRELATIONS", menuPath: "Analyze > Correlate > Bivariate", module: .base, available: true, phase: 4),
            .init(command: "REGRESSION", menuPath: "Analyze > Regression > Linear", module: .base, available: true, phase: 4),
            .init(command: "NPAR TESTS", menuPath: "Analyze > Nonparametric Tests", module: .base, available: true, phase: 4),
            .init(command: "COMPUTE", menuPath: "Transform > Compute Variable", module: .base, available: true, phase: 2),
            .init(command: "RECODE", menuPath: "Transform > Recode", module: .base, available: true, phase: 2),
            .init(command: "BOOTSTRAP", menuPath: "Analyze > Bootstrap", module: .base, available: true, phase: 6),
            .init(command: "UNIANOVA", menuPath: "Analyze > General Linear Model > Univariate", module: .advanced, available: true, phase: 6),
            .init(command: "MIXED", menuPath: "Analyze > Mixed Models > Linear", module: .advanced, available: false, phase: 6),
            .init(command: "FACTOR", menuPath: "Analyze > Dimension Reduction > Factor", module: .advanced, available: true, phase: 6),
            .init(command: "CLUSTER", menuPath: "Analyze > Classify > Hierarchical Cluster", module: .advanced, available: true, phase: 6),
            .init(command: "QUICK CLUSTER", menuPath: "Analyze > Classify > K-Means Cluster", module: .advanced, available: true, phase: 6),
            .init(command: "LOGISTIC REGRESSION", menuPath: "Analyze > Regression > Binary Logistic", module: .regression, available: true, phase: 6),
            .init(command: "KM", menuPath: "Analyze > Survival > Kaplan-Meier", module: .advanced, available: true, phase: 6),
            .init(command: "MULTIPLE IMPUTATION", menuPath: "Analyze > Multiple Imputation", module: .missingValues, available: true, phase: 6),
            .init(command: "TSMODEL", menuPath: "Analyze > Forecasting", module: .forecasting, available: true, phase: 6),
            .init(command: "CTABLES", menuPath: "Analyze > Tables > Custom Tables", module: .customTables, available: true, phase: 7),
            .init(command: "EXACT", menuPath: "Analyze > Exact Tests", module: .exactTests, available: true, phase: 7),
            .init(command: "TREE", menuPath: "Analyze > Classify > Tree", module: .decisionTrees, available: true, phase: 7),
            .init(command: "NEURAL NETWORK", menuPath: "Analyze > Neural Networks", module: .neuralNets, available: true, phase: 7),
            .init(command: "BEGIN PROGRAM", menuPath: "Extensions > Python / R", module: .base, available: true, phase: 8)
        ]
    }

    public func isEnabled(_ command: String) -> Bool {
        guard let cap = all.first(where: { $0.command.caseInsensitiveCompare(command) == .orderedSame }) else {
            return false
        }
        return cap.available && enabledModules.contains(cap.module)
    }

    public var implementedCommands: [Capability] {
        all.filter(\.available)
    }
}
