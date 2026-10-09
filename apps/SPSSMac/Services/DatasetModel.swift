import Foundation
import Combine

public enum MeasureLevel: Int, CaseIterable, Identifiable, Sendable {
    case scale = 0, ordinal = 1, nominal = 2
    public var id: Int { rawValue }
    public var title: String {
        switch self {
        case .scale: return "Scale"
        case .ordinal: return "Ordinal"
        case .nominal: return "Nominal"
        }
    }
}

public struct VariableMeta: Identifiable, Hashable, Sendable {
    public var id: String { name }
    public var name: String
    public var type: String // Numeric / String
    public var width: Int
    public var decimals: Int
    public var label: String
    public var values: String
    public var missing: String
    public var columns: Int
    public var align: String
    public var measure: MeasureLevel
    public var role: String
}

@MainActor
public final class DatasetDocument: ObservableObject, Identifiable {
    public let id = UUID()
    @Published public var displayName: String
    @Published public private(set) var revision: Int = 0
    @Published public var showValueLabels: Bool = false
    @Published public var filterOn: Bool = false
    @Published public var weightOn: Bool = false
    @Published public var splitOn: Bool = false

    public let engine: EngineDataset

    public init(engine: EngineDataset, displayName: String? = nil) {
        self.engine = engine
        self.displayName = displayName ?? engine.name
    }

    public static func empty(name: String = "Dataset1") -> DatasetDocument {
        let eng = EngineDataset(name: name)
        _ = try? eng.addNumeric("VAR00001")
        _ = try? eng.addCases(10)
        return DatasetDocument(engine: eng, displayName: name)
    }

    public var caseCount: Int { engine.caseCount }
    public var variableCount: Int { engine.variableCount }

    public func variableNames() -> [String] {
        (0..<variableCount).map { engine.variableName(col: $0) }
    }

    public func variablesMeta() -> [VariableMeta] {
        (0..<variableCount).map { col in
            let isString = engine.variableType(col: col) == 1
            return VariableMeta(
                name: engine.variableName(col: col),
                type: isString ? "String" : "Numeric",
                width: 8,
                decimals: 2,
                label: engine.variableLabel(col: col),
                values: "",
                missing: "",
                columns: 8,
                align: isString ? "Left" : "Right",
                measure: isString ? .nominal : .scale,
                role: "Input"
            )
        }
    }

    public func cellDisplay(row: Int, col: Int) -> String {
        if engine.variableType(col: col) == 1 {
            return engine.getString(row: row, col: col)
        }
        let v = engine.getNumeric(row: row, col: col)
        if v.isNaN { return "." }
        return String(format: "%.2f", v)
    }

    public func setCell(row: Int, col: Int, text: String) {
        engine.pushUndo()
        if engine.variableType(col: col) == 1 {
            engine.setString(row: row, col: col, value: text)
        } else if text.trimmingCharacters(in: .whitespaces).isEmpty || text == "." {
            engine.setNumeric(row: row, col: col, value: .nan)
        } else if let v = Double(text) {
            engine.setNumeric(row: row, col: col, value: v)
        }
        bump()
    }

    public func bump() { revision += 1 }

    public func undo() { if engine.undo() { bump() } }
    public func redo() { if engine.redo() { bump() } }
}
