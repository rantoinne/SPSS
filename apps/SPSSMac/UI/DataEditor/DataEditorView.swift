import SwiftUI
import AppKit
import SPSSCore

/// Virtualized AppKit grid for Data View (SPSS-like case × variable layout).
struct DataEditorView: NSViewRepresentable {
    @ObservedObject var dataset: DatasetDocument

    func makeNSView(context: Context) -> NSScrollView {
        let scroll = NSScrollView()
        scroll.hasVerticalScroller = true
        scroll.hasHorizontalScroller = true
        scroll.autohidesScrollers = true
        scroll.borderType = .bezelBorder

        let table = NSTableView()
        table.delegate = context.coordinator
        table.dataSource = context.coordinator
        table.allowsColumnResizing = true
        table.allowsColumnReordering = false
        table.usesAlternatingRowBackgroundColors = true
        table.columnAutoresizingStyle = .sequentialColumnAutoresizingStyle
        table.rowHeight = 22
        table.gridStyleMask = [.solidHorizontalGridLineMask, .solidVerticalGridLineMask]
        context.coordinator.table = table
        context.coordinator.rebuildColumns()
        scroll.documentView = table
        return scroll
    }

    func updateNSView(_ nsView: NSScrollView, context: Context) {
        context.coordinator.dataset = dataset
        context.coordinator.rebuildColumnsIfNeeded()
        context.coordinator.table?.reloadData()
    }

    func makeCoordinator() -> Coordinator {
        Coordinator(dataset: dataset)
    }

    @MainActor
    final class Coordinator: NSObject, NSTableViewDelegate, NSTableViewDataSource {
        var dataset: DatasetDocument
        weak var table: NSTableView?
        private var lastVarCount: Int = -1

        init(dataset: DatasetDocument) {
            self.dataset = dataset
        }

        func rebuildColumnsIfNeeded() {
            if dataset.variableCount != lastVarCount {
                rebuildColumns()
            }
        }

        func rebuildColumns() {
            guard let table else { return }
            while table.tableColumns.count > 0 {
                table.removeTableColumn(table.tableColumns[0])
            }
            let rowCol = NSTableColumn(identifier: .init("rownum"))
            rowCol.title = ""
            rowCol.width = 48
            rowCol.minWidth = 40
            table.addTableColumn(rowCol)
            for i in 0..<dataset.variableCount {
                let col = NSTableColumn(identifier: .init("c\(i)"))
                col.title = dataset.engine.variableName(col: i)
                col.width = 80
                col.minWidth = 50
                table.addTableColumn(col)
            }
            lastVarCount = dataset.variableCount
        }

        func numberOfRows(in tableView: NSTableView) -> Int {
            max(dataset.caseCount, 1)
        }

        func tableView(_ tableView: NSTableView, viewFor tableColumn: NSTableColumn?, row: Int) -> NSView? {
            let id = tableColumn?.identifier.rawValue ?? ""
            let field = NSTextField()
            field.isBordered = false
            field.drawsBackground = false
            field.font = .monospacedDigitSystemFont(ofSize: 12, weight: .regular)
            if id == "rownum" {
                field.stringValue = "\(row + 1)"
                field.isEditable = false
                field.alignment = .right
                field.textColor = .secondaryLabelColor
                return field
            }
            guard id.hasPrefix("c"), let col = Int(id.dropFirst()) else { return field }
            if row < dataset.caseCount {
                field.stringValue = dataset.cellDisplay(row: row, col: col)
                field.isEditable = true
                field.tag = row * 10000 + col
                field.target = self
                field.action = #selector(cellEdited(_:))
            } else {
                field.stringValue = ""
                field.isEditable = false
            }
            return field
        }

        @objc func cellEdited(_ sender: NSTextField) {
            let row = sender.tag / 10000
            let col = sender.tag % 10000
            dataset.setCell(row: row, col: col, text: sender.stringValue)
        }
    }
}
