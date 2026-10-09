import SwiftUI
import SPSSCore
import SPSSSyntax

struct FrequenciesDialog: View {
    @EnvironmentObject private var session: ProjectSession
    var onClose: () -> Void
    @State private var selected: Set<String> = []

    var body: some View {
        DialogShell(title: "Frequencies", onOK: ok, onPaste: paste, onReset: { selected = [] }, onCancel: onClose, okEnabled: !selected.isEmpty) {
            variableMultiSelect(selected: $selected)
        }
    }
    private var vars: [String] { Array(selected) }
    private func paste() { session.pasteSyntax(PasteSyntax.frequencies(vars)) }
    private func ok() {
        paste()
        if let ds = session.activeDataset {
            let out = ds.engine.frequencies(vars)
            session.appendOutput(title: "FREQUENCIES", html: out.html)
        }
        onClose()
    }
}

struct DescriptivesDialog: View {
    @EnvironmentObject private var session: ProjectSession
    var onClose: () -> Void
    @State private var selected: Set<String> = []

    var body: some View {
        DialogShell(title: "Descriptives", onOK: ok, onPaste: paste, onReset: { selected = [] }, onCancel: onClose, okEnabled: !selected.isEmpty) {
            variableMultiSelect(selected: $selected)
        }
    }
    private var vars: [String] { Array(selected) }
    private func paste() { session.pasteSyntax(PasteSyntax.descriptives(vars)) }
    private func ok() {
        paste()
        if let ds = session.activeDataset {
            session.appendOutput(title: "DESCRIPTIVES", html: ds.engine.descriptives(vars).html)
        }
        onClose()
    }
}

struct ExploreDialog: View {
    @EnvironmentObject private var session: ProjectSession
    var onClose: () -> Void
    @State private var selected: Set<String> = []

    var body: some View {
        DialogShell(title: "Explore", onOK: ok, onPaste: paste, onReset: { selected = [] }, onCancel: onClose, okEnabled: !selected.isEmpty) {
            variableMultiSelect(selected: $selected)
        }
    }
    private var vars: [String] { Array(selected) }
    private func paste() { session.pasteSyntax(PasteSyntax.explore(vars)) }
    private func ok() {
        paste()
        if let ds = session.activeDataset {
            session.appendOutput(title: "EXAMINE", html: ds.engine.descriptives(vars).html)
        }
        onClose()
    }
}

struct CrosstabsDialog: View {
    @EnvironmentObject private var session: ProjectSession
    var onClose: () -> Void
    @State private var row = ""
    @State private var col = ""

    var body: some View {
        DialogShell(title: "Crosstabs", onOK: ok, onPaste: paste, onReset: {}, onCancel: onClose, okEnabled: !row.isEmpty && !col.isEmpty) {
            variablePicker("Row", selection: $row)
            variablePicker("Column", selection: $col)
        }
        .onAppear {
            let names = session.activeDataset?.variableNames() ?? []
            row = names.first ?? ""
            col = names.dropFirst().first ?? names.first ?? ""
        }
    }
    private func paste() { session.pasteSyntax(PasteSyntax.crosstabs(row: row, col: col)) }
    private func ok() {
        paste()
        if let ds = session.activeDataset {
            session.appendOutput(title: "CROSSTABS", html: ds.engine.crosstabs(row: row, col: col).html)
        }
        onClose()
    }
}

struct TTestDialog: View {
    @EnvironmentObject private var session: ProjectSession
    var onClose: () -> Void
    @State private var variable = ""
    @State private var testValue = "0"

    var body: some View {
        DialogShell(title: "One-Sample T Test", onOK: ok, onPaste: paste, onReset: { testValue = "0" }, onCancel: onClose) {
            variablePicker("Test Variable", selection: $variable)
            TextField("Test Value", text: $testValue)
        }
        .onAppear { variable = session.activeDataset?.variableNames().first ?? "" }
    }
    private func paste() {
        session.pasteSyntax(PasteSyntax.ttest(variable: variable, testValue: Double(testValue) ?? 0))
    }
    private func ok() {
        paste()
        if let ds = session.activeDataset {
            session.appendOutput(title: "T-TEST", html: ds.engine.ttestOneSample(variable: variable, testValue: Double(testValue) ?? 0).html)
        }
        onClose()
    }
}

struct OnewayDialog: View {
    @EnvironmentObject private var session: ProjectSession
    var onClose: () -> Void
    @State private var dependent = ""
    @State private var factor = ""

    var body: some View {
        DialogShell(title: "One-Way ANOVA", onOK: ok, onPaste: paste, onReset: {}, onCancel: onClose) {
            variablePicker("Dependent", selection: $dependent)
            variablePicker("Factor", selection: $factor)
        }
        .onAppear {
            let names = session.activeDataset?.variableNames() ?? []
            dependent = names.first ?? ""
            factor = names.dropFirst().first ?? ""
        }
    }
    private func paste() { session.pasteSyntax(PasteSyntax.oneway(dependent: dependent, factor: factor)) }
    private func ok() {
        paste()
        if let ds = session.activeDataset {
            session.appendOutput(title: "ONEWAY", html: ds.engine.oneway(dependent: dependent, factor: factor).html)
        }
        onClose()
    }
}

struct CorrelationsDialog: View {
    @EnvironmentObject private var session: ProjectSession
    var onClose: () -> Void
    @State private var selected: Set<String> = []

    var body: some View {
        DialogShell(title: "Bivariate Correlations", onOK: ok, onPaste: paste, onReset: { selected = [] }, onCancel: onClose, okEnabled: selected.count >= 2) {
            variableMultiSelect(selected: $selected)
        }
    }
    private var vars: [String] { Array(selected) }
    private func paste() { session.pasteSyntax(PasteSyntax.correlations(vars)) }
    private func ok() {
        paste()
        if let ds = session.activeDataset {
            session.appendOutput(title: "CORRELATIONS", html: ds.engine.correlation(vars).html)
        }
        onClose()
    }
}

struct RegressionDialog: View {
    @EnvironmentObject private var session: ProjectSession
    var onClose: () -> Void
    @State private var dependent = ""
    @State private var selected: Set<String> = []

    var body: some View {
        DialogShell(title: "Linear Regression", onOK: ok, onPaste: paste, onReset: { selected = [] }, onCancel: onClose, okEnabled: !dependent.isEmpty && !selected.isEmpty) {
            variablePicker("Dependent", selection: $dependent)
            Text("Independents").font(.subheadline)
            variableMultiSelect(selected: $selected)
        }
        .onAppear { dependent = session.activeDataset?.variableNames().first ?? "" }
    }
    private var indep: [String] { Array(selected) }
    private func paste() { session.pasteSyntax(PasteSyntax.regression(dependent: dependent, independents: indep)) }
    private func ok() {
        paste()
        if let ds = session.activeDataset {
            session.appendOutput(title: "REGRESSION", html: ds.engine.regression(dependent: dependent, independents: indep).html)
        }
        onClose()
    }
}

struct GLMDialog: View {
    @EnvironmentObject private var session: ProjectSession
    @EnvironmentObject private var capabilities: CapabilityRegistry
    var onClose: () -> Void
    @State private var dependent = ""
    @State private var factor = ""
    @State private var covariate = ""

    var body: some View {
        DialogShell(title: "Univariate GLM", onOK: ok, onPaste: paste, onReset: {}, onCancel: onClose,
                    okEnabled: capabilities.isEnabled("UNIANOVA") && !dependent.isEmpty && !factor.isEmpty) {
            if !capabilities.isEnabled("UNIANOVA") {
                Text("Module not enabled (Advanced Statistics).").foregroundStyle(.secondary)
            }
            variablePicker("Dependent", selection: $dependent)
            variablePicker("Fixed Factor", selection: $factor)
            variablePicker("Covariate (optional)", selection: $covariate)
        }
        .onAppear {
            let names = session.activeDataset?.variableNames() ?? []
            dependent = names.first ?? ""
            factor = names.dropFirst().first ?? ""
            covariate = names.dropFirst(2).first ?? ""
        }
    }
    private func paste() {
        let covs = covariate.isEmpty ? [] : [covariate]
        session.pasteSyntax(PasteSyntax.unianova(dependent: dependent, factors: [factor], covariates: covs))
    }
    private func ok() {
        paste()
        if let ds = session.activeDataset {
            let covs = covariate.isEmpty ? [] : [covariate]
            session.appendOutput(title: "UNIANOVA", html: ds.engine.glm(dependent: dependent, factors: [factor], covariates: covs).html)
        }
        onClose()
    }
}

struct FactorDialog: View {
    @EnvironmentObject private var session: ProjectSession
    var onClose: () -> Void
    @State private var selected: Set<String> = []

    var body: some View {
        DialogShell(title: "Factor Analysis (PCA)", onOK: ok, onPaste: paste, onReset: { selected = [] }, onCancel: onClose, okEnabled: selected.count >= 2) {
            variableMultiSelect(selected: $selected)
        }
    }
    private var vars: [String] { Array(selected) }
    private func paste() { session.pasteSyntax(PasteSyntax.factor(vars)) }
    private func ok() {
        paste()
        if let ds = session.activeDataset {
            session.appendOutput(title: "FACTOR", html: ds.engine.factor(vars).html)
        }
        onClose()
    }
}

struct ClusterDialog: View {
    @EnvironmentObject private var session: ProjectSession
    var onClose: () -> Void
    @State private var selected: Set<String> = []
    @State private var k = "2"

    var body: some View {
        DialogShell(title: "K-Means Cluster", onOK: ok, onPaste: paste, onReset: { selected = []; k = "2" }, onCancel: onClose, okEnabled: !selected.isEmpty) {
            variableMultiSelect(selected: $selected)
            TextField("Number of Clusters", text: $k)
        }
    }
    private var vars: [String] { Array(selected) }
    private func paste() { session.pasteSyntax(PasteSyntax.cluster(vars, k: Int(k) ?? 2)) }
    private func ok() {
        paste()
        if let ds = session.activeDataset {
            session.appendOutput(title: "QUICK CLUSTER", html: ds.engine.cluster(vars, k: Int(k) ?? 2).html)
        }
        onClose()
    }
}

struct LogisticDialog: View {
    @EnvironmentObject private var session: ProjectSession
    var onClose: () -> Void
    @State private var dependent = ""
    @State private var selected: Set<String> = []

    var body: some View {
        DialogShell(title: "Binary Logistic Regression", onOK: ok, onPaste: paste, onReset: { selected = [] }, onCancel: onClose, okEnabled: !dependent.isEmpty && !selected.isEmpty) {
            variablePicker("Dependent", selection: $dependent)
            Text("Covariates").font(.subheadline)
            variableMultiSelect(selected: $selected)
        }
        .onAppear { dependent = session.activeDataset?.variableNames().first ?? "" }
    }
    private var indep: [String] { Array(selected) }
    private func paste() { session.pasteSyntax(PasteSyntax.logistic(dependent: dependent, independents: indep)) }
    private func ok() {
        paste()
        if let ds = session.activeDataset {
            session.appendOutput(title: "LOGISTIC REGRESSION", html: ds.engine.logistic(dependent: dependent, independents: indep).html)
        }
        onClose()
    }
}

struct KMDialog: View {
    @EnvironmentObject private var session: ProjectSession
    var onClose: () -> Void
    @State private var time = ""
    @State private var status = ""

    var body: some View {
        DialogShell(title: "Kaplan-Meier", onOK: ok, onPaste: paste, onReset: {}, onCancel: onClose, okEnabled: !time.isEmpty && !status.isEmpty) {
            variablePicker("Time", selection: $time)
            variablePicker("Status", selection: $status)
        }
        .onAppear {
            let names = session.activeDataset?.variableNames() ?? []
            time = names.first ?? ""
            status = names.dropFirst().first ?? ""
        }
    }
    private func paste() { session.pasteSyntax(PasteSyntax.km(time: time, status: status)) }
    private func ok() {
        paste()
        if let ds = session.activeDataset {
            session.appendOutput(title: "KM", html: ds.engine.kaplanMeier(time: time, status: status).html)
        }
        onClose()
    }
}

struct CTablesDialog: View {
    @EnvironmentObject private var session: ProjectSession
    var onClose: () -> Void
    @State private var row = ""
    @State private var col = ""

    var body: some View {
        DialogShell(title: "Custom Tables", onOK: ok, onPaste: paste, onReset: {}, onCancel: onClose, okEnabled: !row.isEmpty && !col.isEmpty) {
            variablePicker("Row", selection: $row)
            variablePicker("Column", selection: $col)
        }
        .onAppear {
            let names = session.activeDataset?.variableNames() ?? []
            row = names.first ?? ""
            col = names.dropFirst().first ?? names.first ?? ""
        }
    }
    private func paste() { session.pasteSyntax(PasteSyntax.ctables(row: row, column: col)) }
    private func ok() {
        paste()
        if let ds = session.activeDataset {
            session.appendOutput(title: "CTABLES", html: ds.engine.ctables(row: row, column: col).html)
        }
        onClose()
    }
}

struct TreeDialog: View {
    @EnvironmentObject private var session: ProjectSession
    var onClose: () -> Void
    @State private var dependent = ""
    @State private var predictor = ""

    var body: some View {
        DialogShell(title: "Decision Tree", onOK: ok, onPaste: paste, onReset: {}, onCancel: onClose, okEnabled: !dependent.isEmpty && !predictor.isEmpty) {
            variablePicker("Dependent", selection: $dependent)
            variablePicker("Predictor", selection: $predictor)
        }
        .onAppear {
            let names = session.activeDataset?.variableNames() ?? []
            dependent = names.first ?? ""
            predictor = names.dropFirst().first ?? ""
        }
    }
    private func paste() { session.pasteSyntax(PasteSyntax.tree(dependent: dependent, predictor: predictor)) }
    private func ok() {
        paste()
        if let ds = session.activeDataset {
            session.appendOutput(title: "TREE", html: ds.engine.tree(dependent: dependent, predictor: predictor).html)
        }
        onClose()
    }
}

struct VariableMultiSelect: View {
    @EnvironmentObject private var session: ProjectSession
    @Binding var selected: Set<String>
    var body: some View {
        List(session.activeDataset?.variableNames() ?? [], id: \.self) { name in
            Toggle(isOn: Binding(
                get: { selected.contains(name) },
                set: { on in
                    if on { selected.insert(name) } else { selected.remove(name) }
                }
            )) {
                Text(name)
            }
            .toggleStyle(.checkbox)
        }
        .frame(minHeight: 160)
    }
}

@ViewBuilder
func variableMultiSelect(selected: Binding<Set<String>>) -> some View {
    VariableMultiSelect(selected: selected)
}
