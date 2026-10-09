import Foundation

/// Generates SPSS-like syntax from dialog field values (Paste button).
public enum PasteSyntax {
    public static func compute(target: String, expression: String) -> String {
        "COMPUTE \(target) = \(expression).\nEXECUTE.\n"
    }

    public static func recode(source: String, target: String, pairs: [(String, String)]) -> String {
        let body = pairs.map { "(\($0.0)=\($0.1))" }.joined(separator: " ")
        if source == target {
            return "RECODE \(source) \(body).\nEXECUTE.\n"
        }
        return "RECODE \(source) \(body) INTO \(target).\nEXECUTE.\n"
    }

    public static func autoRecode(source: String, target: String) -> String {
        "AUTORECODE VARIABLES=\(source) /INTO \(target).\nEXECUTE.\n"
    }

    public static func rank(source: String, target: String) -> String {
        "RANK VARIABLES=\(source) /RANK INTO \(target).\nEXECUTE.\n"
    }

    public static func sort(variable: String, ascending: Bool) -> String {
        let order = ascending ? "A" : "D"
        return "SORT CASES BY \(variable)(\(order)).\n"
    }

    public static func selectIf(_ expression: String) -> String {
        "SELECT IF (\(expression)).\nEXECUTE.\n"
    }

    public static func weight(variable: String?) -> String {
        if let variable { return "WEIGHT BY \(variable).\n" }
        return "WEIGHT OFF.\n"
    }

    public static func split(variables: [String]) -> String {
        if variables.isEmpty { return "SPLIT FILE OFF.\n" }
        return "SPLIT FILE BY \(variables.joined(separator: " ")).\n"
    }

    public static func frequencies(_ variables: [String]) -> String {
        "FREQUENCIES VARIABLES=\(variables.joined(separator: " "))\n  /ORDER=ANALYSIS.\n"
    }

    public static func descriptives(_ variables: [String]) -> String {
        "DESCRIPTIVES VARIABLES=\(variables.joined(separator: " "))\n  /STATISTICS=MEAN STDDEV MIN MAX.\n"
    }

    public static func explore(_ variables: [String]) -> String {
        "EXAMINE VARIABLES=\(variables.joined(separator: " "))\n  /PLOT BOXPLOT HISTOGRAM.\n"
    }

    public static func crosstabs(row: String, col: String) -> String {
        "CROSSTABS\n  /TABLES=\(row) BY \(col)\n  /STATISTICS=CHISQ.\n"
    }

    public static func ttest(variable: String, testValue: Double) -> String {
        "T-TEST\n  /TESTVAL=\(testValue)\n  /VARIABLES=\(variable).\n"
    }

    public static func oneway(dependent: String, factor: String) -> String {
        "ONEWAY \(dependent) BY \(factor).\n"
    }

    public static func correlations(_ variables: [String]) -> String {
        "CORRELATIONS\n  /VARIABLES=\(variables.joined(separator: " "))\n  /PRINT=TWOTAIL NOSIG.\n"
    }

    public static func regression(dependent: String, independents: [String]) -> String {
        "REGRESSION\n  /DEPENDENT \(dependent)\n  /METHOD=ENTER \(independents.joined(separator: " ")).\n"
    }

    public static func unianova(dependent: String, factors: [String], covariates: [String] = []) -> String {
        var s = "UNIANOVA \(dependent) BY \(factors.joined(separator: " "))"
        if !covariates.isEmpty { s += " WITH \(covariates.joined(separator: " "))" }
        return s + ".\n"
    }

    public static func factor(_ variables: [String], nFactors: Int = 0) -> String {
        var s = "FACTOR\n  /VARIABLES \(variables.joined(separator: " "))\n  /EXTRACTION PC"
        if nFactors > 0 { s += "\n  /CRITERIA=FACTORS(\(nFactors))" }
        return s + ".\n"
    }

    public static func cluster(_ variables: [String], k: Int) -> String {
        "QUICK CLUSTER \(variables.joined(separator: " "))\n  /CRITERIA=CLUSTER(\(k)).\n"
    }

    public static func logistic(dependent: String, independents: [String]) -> String {
        "LOGISTIC REGRESSION VARIABLES=\(dependent)\n  /METHOD=ENTER \(independents.joined(separator: " ")).\n"
    }

    public static func km(time: String, status: String) -> String {
        "KM \(time) STATUS \(status)(1).\n"
    }

    public static func impute(_ variables: [String], m: Int = 5) -> String {
        "MULTIPLE IMPUTATION VARIABLES=\(variables.joined(separator: " "))\n  /NIMPUTATIONS=\(m).\n"
    }

    public static func bootstrap(_ variables: [String], samples: Int = 200) -> String {
        "BOOTSTRAP\n  /VARIABLES \(variables.joined(separator: " "))\n  /SAMPLES=\(samples).\n"
    }

    public static func forecast(variable: String, periods: Int = 3) -> String {
        "TSMODEL\n  /VARIABLES \(variable)\n  /PERIODS=\(periods).\n"
    }

    public static func ctables(row: String, column: String) -> String {
        "CTABLES\n  /TABLE \(row) BY \(column).\n"
    }

    public static func tree(dependent: String, predictor: String) -> String {
        "TREE \(dependent) BY \(predictor).\n"
    }

    public static func exact(row: String, column: String) -> String {
        "EXACT TABLES=\(row) BY \(column).\n"
    }

    public static func neural(dependent: String, independents: [String]) -> String {
        "NEURAL NETWORK\n  /DEPENDENT \(dependent)\n  /FACTORS \(independents.joined(separator: " ")).\n"
    }
}
