import SwiftUI
import SPSSCore
import SPSSSyntax

struct ComputeDialog: View {
    @EnvironmentObject private var session: ProjectSession
    var onClose: () -> Void
    @State private var target = "NEWVAR"
    @State private var expression = ""

    var body: some View {
        DialogShell(title: "Compute Variable", onOK: ok, onPaste: paste, onReset: reset, onCancel: onClose, okEnabled: !expression.isEmpty) {
            TextField("Target Variable", text: $target)
            TextField("Numeric Expression", text: $expression)
            Text("Example: age + score").font(.caption).foregroundStyle(.secondary)
        }
    }

    private func paste() { session.pasteSyntax(PasteSyntax.compute(target: target, expression: expression)) }
    private func reset() { target = "NEWVAR"; expression = "" }
    private func ok() {
        paste()
        try? session.activeDataset?.engine.compute(target: target, expression: expression)
        session.activeDataset?.bump()
        session.statusMessage = "COMPUTE \(target)"
        onClose()
    }
}

struct RecodeDialog: View {
    @EnvironmentObject private var session: ProjectSession
    var onClose: () -> Void
    @State private var source = ""
    @State private var target = ""
    @State private var from = "1"
    @State private var to = "2"

    var body: some View {
        DialogShell(title: "Recode into Different Variables", onOK: ok, onPaste: paste, onReset: reset, onCancel: onClose) {
            variablePicker("Input", selection: $source)
            TextField("Output Variable", text: $target)
            HStack {
                TextField("Old Value", text: $from)
                TextField("New Value", text: $to)
            }
        }
        .onAppear {
            source = session.activeDataset?.variableNames().first ?? ""
            target = source.isEmpty ? "RVAR" : source + "_r"
        }
    }

    private func paste() {
        session.pasteSyntax(PasteSyntax.recode(source: source, target: target, pairs: [(from, to)]))
    }
    private func reset() { from = "1"; to = "2" }
    private func ok() {
        paste()
        if let f = Double(from), let t = Double(to) {
            try? session.activeDataset?.engine.recodeValue(source: source, target: target, from: f, to: t)
            session.activeDataset?.bump()
        }
        onClose()
    }
}

struct AutoRecodeDialog: View {
    @EnvironmentObject private var session: ProjectSession
    var onClose: () -> Void
    @State private var source = ""
    @State private var target = ""

    var body: some View {
        DialogShell(title: "Automatic Recode", onOK: { paste(); onClose() }, onPaste: paste, onReset: {}, onCancel: onClose) {
            variablePicker("Variable", selection: $source)
            TextField("New Name", text: $target)
        }
        .onAppear {
            source = session.activeDataset?.variableNames().first ?? ""
            target = "AR_" + source
        }
    }

    private func paste() {
        session.pasteSyntax(PasteSyntax.autoRecode(source: source, target: target))
    }
}

struct RankDialog: View {
    @EnvironmentObject private var session: ProjectSession
    var onClose: () -> Void
    @State private var source = ""
    @State private var target = ""

    var body: some View {
        DialogShell(title: "Rank Cases", onOK: { paste(); onClose() }, onPaste: paste, onReset: {}, onCancel: onClose) {
            variablePicker("Variable", selection: $source)
            TextField("Rank into", text: $target)
        }
        .onAppear {
            source = session.activeDataset?.variableNames().first ?? ""
            target = "R" + source
        }
    }

    private func paste() {
        session.pasteSyntax(PasteSyntax.rank(source: source, target: target))
    }
}

struct SortDialog: View {
    @EnvironmentObject private var session: ProjectSession
    var onClose: () -> Void
    @State private var variable = ""
    @State private var ascending = true

    var body: some View {
        DialogShell(title: "Sort Cases", onOK: ok, onPaste: paste, onReset: { ascending = true }, onCancel: onClose) {
            variablePicker("Sort by", selection: $variable)
            Toggle("Ascending", isOn: $ascending)
        }
        .onAppear { variable = session.activeDataset?.variableNames().first ?? "" }
    }

    private func paste() { session.pasteSyntax(PasteSyntax.sort(variable: variable, ascending: ascending)) }
    private func ok() {
        paste()
        try? session.activeDataset?.engine.sort(variable: variable, ascending: ascending)
        session.activeDataset?.bump()
        onClose()
    }
}

struct SelectCasesDialog: View {
    @EnvironmentObject private var session: ProjectSession
    var onClose: () -> Void
    @State private var expression = ""

    var body: some View {
        DialogShell(title: "Select Cases", onOK: ok, onPaste: paste, onReset: { expression = "" }, onCancel: onClose, okEnabled: !expression.isEmpty) {
            TextField("Condition (e.g. age > 25)", text: $expression)
            Text("Applies FILTER BY filter_$").font(.caption).foregroundStyle(.secondary)
        }
    }

    private func paste() { session.pasteSyntax(PasteSyntax.selectIf(expression)) }
    private func ok() {
        paste()
        try? session.activeDataset?.engine.filterSelect(expression: expression)
        session.activeDataset?.filterOn = true
        session.activeDataset?.bump()
        onClose()
    }
}

struct WeightDialog: View {
    @EnvironmentObject private var session: ProjectSession
    var onClose: () -> Void
    @State private var variable = ""
    @State private var off = false

    var body: some View {
        DialogShell(title: "Weight Cases", onOK: ok, onPaste: paste, onReset: { off = false }, onCancel: onClose) {
            Toggle("Do not weight cases", isOn: $off)
            if !off { variablePicker("Frequency variable", selection: $variable) }
        }
        .onAppear { variable = session.activeDataset?.variableNames().first ?? "" }
    }

    private func paste() { session.pasteSyntax(PasteSyntax.weight(variable: off ? nil : variable)) }
    private func ok() {
        paste()
        try? session.activeDataset?.engine.weight(by: off ? nil : variable)
        session.activeDataset?.weightOn = !off
        session.activeDataset?.bump()
        onClose()
    }
}

struct SplitFileDialog: View {
    @EnvironmentObject private var session: ProjectSession
    var onClose: () -> Void
    @State private var variable = ""
    @State private var off = false

    var body: some View {
        DialogShell(title: "Split File", onOK: ok, onPaste: paste, onReset: { off = false }, onCancel: onClose) {
            Toggle("Analyze all cases, do not create groups", isOn: $off)
            if !off { variablePicker("Groups based on", selection: $variable) }
        }
        .onAppear { variable = session.activeDataset?.variableNames().first ?? "" }
    }

    private func paste() { session.pasteSyntax(PasteSyntax.split(variables: off ? [] : [variable])) }
    private func ok() {
        paste()
        session.activeDataset?.splitOn = !off
        session.activeDataset?.bump()
        onClose()
    }
}

@ViewBuilder
func variablePicker(_ title: String, selection: Binding<String>) -> some View {
    VariablePicker(title: title, selection: selection)
}

private struct VariablePicker: View {
    @EnvironmentObject private var session: ProjectSession
    let title: String
    @Binding var selection: String
    var body: some View {
        Picker(title, selection: $selection) {
            ForEach(session.activeDataset?.variableNames() ?? [], id: \.self) { Text($0).tag($0) }
        }
    }
}
