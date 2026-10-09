import Foundation
import SPSSEngineC

public enum EngineError: Error, LocalizedError {
    case message(String)
    public var errorDescription: String? {
        switch self {
        case .message(let s): return s
        }
    }
}

/// Thin Swift wrapper around the C engine API.
public final class EngineDataset: @unchecked Sendable {
    private var ptr: OpaquePointer?

    public init(name: String = "Dataset1") {
        ptr = spss_dataset_create(name)
    }

    init(adopting pointer: OpaquePointer?) {
        ptr = pointer
    }

    deinit {
        if let ptr { spss_dataset_destroy(ptr) }
    }

    public var raw: OpaquePointer? { ptr }

    public var name: String {
        String(cString: spss_dataset_name(ptr))
    }

    public var caseCount: Int { Int(spss_dataset_case_count(ptr)) }
    public var variableCount: Int { Int(spss_dataset_var_count(ptr)) }

    public func addNumeric(_ name: String, width: Int = 8, decimals: Int = 2) throws -> Int {
        let i = spss_dataset_add_numeric(ptr, name, Int32(width), Int32(decimals))
        if i < 0 { throw EngineError.message(Self.lastError()) }
        return Int(i)
    }

    public func addString(_ name: String, width: Int = 8) throws -> Int {
        let i = spss_dataset_add_string(ptr, name, Int32(width))
        if i < 0 { throw EngineError.message(Self.lastError()) }
        return Int(i)
    }

    @discardableResult
    public func addCases(_ n: Int) throws -> Int {
        let i = spss_dataset_add_cases(ptr, n)
        if i < 0 { throw EngineError.message(Self.lastError()) }
        return Int(i)
    }

    public func getNumeric(row: Int, col: Int) -> Double {
        spss_get_numeric(ptr, row, col)
    }

    public func setNumeric(row: Int, col: Int, value: Double) {
        spss_set_numeric(ptr, row, col, value)
    }

    public func getString(row: Int, col: Int) -> String {
        String(cString: spss_get_string(ptr, row, col))
    }

    public func setString(row: Int, col: Int, value: String) {
        spss_set_string(ptr, row, col, value)
    }

    public func variableName(col: Int) -> String {
        String(cString: spss_var_name(ptr, col))
    }

    public func variableLabel(col: Int) -> String {
        String(cString: spss_var_label(ptr, col))
    }

    public func setVariableLabel(col: Int, _ label: String) {
        spss_set_var_label(ptr, col, label)
    }

    public func variableType(col: Int) -> Int {
        Int(spss_var_type(ptr, col))
    }

    public func pushUndo() { spss_push_undo(ptr) }
    @discardableResult public func undo() -> Bool { spss_undo(ptr) != 0 }
    @discardableResult public func redo() -> Bool { spss_redo(ptr) != 0 }

    public func compute(target: String, expression: String) throws {
        if spss_compute(ptr, target, expression) != 0 {
            throw EngineError.message(Self.lastError())
        }
    }

    public func recodeValue(source: String, target: String, from: Double, to: Double) throws {
        if spss_recode_value(ptr, source, target, from, to) != 0 {
            throw EngineError.message(Self.lastError())
        }
    }

    public func sort(variable: String, ascending: Bool) throws {
        if spss_sort(ptr, variable, ascending ? 1 : 0) != 0 {
            throw EngineError.message(Self.lastError())
        }
    }

    public func weight(by variable: String?) throws {
        if spss_weight(ptr, variable) != 0 {
            throw EngineError.message(Self.lastError())
        }
    }

    public func filterSelect(expression: String) throws {
        if spss_filter_select(ptr, expression) != 0 {
            throw EngineError.message(Self.lastError())
        }
    }

    public static func readCSV(path: String, header: Bool = true) throws -> EngineDataset {
        var out: OpaquePointer?
        if spss_read_csv(&out, path, header ? 1 : 0) != 0 {
            throw EngineError.message(lastError())
        }
        return EngineDataset(adopting: out)
    }

    public func writeCSV(path: String) throws {
        if spss_write_csv(ptr, path) != 0 {
            throw EngineError.message(Self.lastError())
        }
    }

    public static func readSAV(path: String) throws -> EngineDataset {
        var out: OpaquePointer?
        if spss_read_sav(&out, path) != 0 {
            throw EngineError.message(lastError())
        }
        return EngineDataset(adopting: out)
    }

    public func writeSAV(path: String) throws {
        if spss_write_sav(ptr, path) != 0 {
            throw EngineError.message(Self.lastError())
        }
    }

    public func frequencies(_ vars: [String]) -> EngineOutput {
        vars.withCStrings { ptrs in
            EngineOutput(adopting: spss_frequencies(ptr, ptrs, vars.count))
        }
    }

    public func descriptives(_ vars: [String]) -> EngineOutput {
        vars.withCStrings { ptrs in
            EngineOutput(adopting: spss_descriptives(ptr, ptrs, vars.count))
        }
    }

    public func crosstabs(row: String, col: String) -> EngineOutput {
        EngineOutput(adopting: spss_crosstabs(ptr, row, col))
    }

    public func ttestOneSample(variable: String, testValue: Double) -> EngineOutput {
        EngineOutput(adopting: spss_ttest_onesample(ptr, variable, testValue))
    }

    public func correlation(_ vars: [String]) -> EngineOutput {
        vars.withCStrings { ptrs in
            EngineOutput(adopting: spss_correlation(ptr, ptrs, vars.count))
        }
    }

    public func regression(dependent: String, independents: [String]) -> EngineOutput {
        independents.withCStrings { ptrs in
            EngineOutput(adopting: spss_regression(ptr, dependent, ptrs, independents.count))
        }
    }

    public func oneway(dependent: String, factor: String) -> EngineOutput {
        EngineOutput(adopting: spss_oneway(ptr, dependent, factor))
    }

    public func glm(dependent: String, factors: [String], covariates: [String] = []) -> EngineOutput {
        factors.withCStrings { fptrs in
            if covariates.isEmpty {
                return EngineOutput(adopting: spss_glm(ptr, dependent, fptrs, factors.count, nil, 0))
            }
            return covariates.withCStrings { cptrs in
                EngineOutput(adopting: spss_glm(ptr, dependent, fptrs, factors.count, cptrs, covariates.count))
            }
        }
    }

    public func factor(_ vars: [String], nFactors: Int = 0) -> EngineOutput {
        vars.withCStrings { ptrs in
            EngineOutput(adopting: spss_factor(ptr, ptrs, vars.count, Int32(nFactors)))
        }
    }

    public func cluster(_ vars: [String], k: Int = 2) -> EngineOutput {
        vars.withCStrings { ptrs in
            EngineOutput(adopting: spss_cluster(ptr, ptrs, vars.count, Int32(k)))
        }
    }

    public func logistic(dependent: String, independents: [String]) -> EngineOutput {
        independents.withCStrings { ptrs in
            EngineOutput(adopting: spss_logistic(ptr, dependent, ptrs, independents.count))
        }
    }

    public func kaplanMeier(time: String, status: String) -> EngineOutput {
        EngineOutput(adopting: spss_km(ptr, time, status))
    }

    public func impute(_ vars: [String], m: Int = 5) -> EngineOutput {
        vars.withCStrings { ptrs in
            EngineOutput(adopting: spss_impute(ptr, ptrs, vars.count, Int32(m)))
        }
    }

    public func bootstrap(_ vars: [String], samples: Int = 200) -> EngineOutput {
        vars.withCStrings { ptrs in
            EngineOutput(adopting: spss_bootstrap(ptr, ptrs, vars.count, Int32(samples)))
        }
    }

    public func forecast(variable: String, periods: Int = 3, window: Int = 3) -> EngineOutput {
        EngineOutput(adopting: spss_forecast(ptr, variable, Int32(periods), Int32(window)))
    }

    public func ctables(row: String, column: String, summary: String? = nil) -> EngineOutput {
        if let summary {
            return EngineOutput(adopting: spss_ctables(ptr, row, column, summary))
        }
        return EngineOutput(adopting: spss_ctables(ptr, row, column, nil))
    }

    public func tree(dependent: String, predictor: String) -> EngineOutput {
        EngineOutput(adopting: spss_tree(ptr, dependent, predictor))
    }

    public func exact(row: String, column: String) -> EngineOutput {
        EngineOutput(adopting: spss_exact(ptr, row, column))
    }

    public func neural(dependent: String, independents: [String]) -> EngineOutput {
        independents.withCStrings { ptrs in
            EngineOutput(adopting: spss_neural(ptr, dependent, ptrs, independents.count))
        }
    }

    public func clone() -> EngineDataset {
        EngineDataset(adopting: spss_dataset_clone(ptr))
    }

    private static func lastError() -> String {
        String(cString: spss_last_error())
    }
}

public final class EngineOutput: @unchecked Sendable {
    private var ptr: OpaquePointer?
    public init(adopting pointer: OpaquePointer?) { ptr = pointer }
    deinit { if let ptr { spss_output_destroy(ptr) } }
    public var itemCount: Int { Int(spss_output_item_count(ptr)) }
    public var html: String { String(cString: spss_output_html(ptr)) }
}

private extension Array where Element == String {
    func withCStrings<R>(_ body: (UnsafePointer<UnsafePointer<CChar>?>?) -> R) -> R {
        if isEmpty { return body(nil) }
        let cStrings = self.map { strdup($0) }
        defer { cStrings.forEach { free($0) } }
        return cStrings.withUnsafeBufferPointer { buf in
            buf.baseAddress!.withMemoryRebound(to: UnsafePointer<CChar>?.self, capacity: buf.count) { ptr in
                body(ptr)
            }
        }
    }
}
