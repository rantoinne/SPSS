import SwiftUI
import AppKit
import SPSSCore

/// Variable View with SPSS column order: Name Type Width Decimals Label Values Missing Columns Align Measure Role
struct VariableView: NSViewRepresentable {
    @ObservedObject var dataset: DatasetDocument

    private static let columns = [
        "Name", "Type", "Width", "Decimals", "Label", "Values",
        "Missing", "Columns", "Align", "Measure", "Role"
    ]

    func makeNSView(context: Context) -> NSScrollView {
        let scroll = NSScrollView()
        scroll.hasVerticalScroller = true
        scroll.hasHorizontalScroller = true
        scroll.borderType = .bezelBorder
        let table = NSTableView()
        table.delegate = context.coordinator
        table.dataSource = context.coordinator
        table.usesAlternatingRowBackgroundColors = true
        table.rowHeight = 24
        table.gridStyleMask = [.solidHorizontalGridLineMask, .solidVerticalGridLineMask]
        for title in Self.columns {
            let col = NSTableColumn(identifier: .init(title))
            col.title = title
            col.width = title == "Label" ? 160 : (title == "Name" ? 100 : 80)
            table.addTableColumn(col)
        }
        context.coordinator.table = table
        scroll.documentView = table
        return scroll
    }

    func updateNSView(_ nsView: NSScrollView, context: Context) {
        context.coordinator.dataset = dataset
        context.coordinator.table?.reloadData()
    }

    func makeCoordinator() -> Coordinator { Coordinator(dataset: dataset) }

    @MainActor
    final class Coordinator: NSObject, NSTableViewDelegate, NSTableViewDataSource {
        var dataset: DatasetDocument
        weak var table: NSTableView?
        init(dataset: DatasetDocument) { self.dataset = dataset }

        func numberOfRows(in tableView: NSTableView) -> Int { dataset.variableCount }

        func tableView(_ tableView: NSTableView, viewFor tableColumn: NSTableColumn?, row: Int) -> NSView? {
            let meta = dataset.variablesMeta()
            guard row < meta.count, let key = tableColumn?.identifier.rawValue else { return nil }
            let v = meta[row]
            let field = NSTextField()
            field.isBordered = false
            field.drawsBackground = false
            field.font = .systemFont(ofSize: 12)
            switch key {
            case "Name": field.stringValue = v.name; field.isEditable = false
            case "Type": field.stringValue = v.type; field.isEditable = false
            case "Width": field.stringValue = "\(v.width)"
            case "Decimals": field.stringValue = "\(v.decimals)"
            case "Label":
                field.stringValue = v.label
                field.isEditable = true
                field.tag = row
                field.target = self
                field.action = #selector(labelEdited(_:))
            case "Values": field.stringValue = v.values
            case "Missing": field.stringValue = v.missing
            case "Columns": field.stringValue = "\(v.columns)"
            case "Align": field.stringValue = v.align
            case "Measure": field.stringValue = v.measure.title
            case "Role": field.stringValue = v.role
            default: field.stringValue = ""
            }
            if key != "Label" { field.isEditable = false }
            return field
        }

        @objc func labelEdited(_ sender: NSTextField) {
            dataset.engine.setVariableLabel(col: sender.tag, sender.stringValue)
            dataset.bump()
        }
    }
}
