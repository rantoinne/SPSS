import SwiftUI
import AppKit
import UniformTypeIdentifiers
import SPSSCore

struct ContentView: View {
    @EnvironmentObject private var session: ProjectSession
    @EnvironmentObject private var capabilities: CapabilityRegistry
    @State private var editorTab: EditorTab = .data
    @State private var showImporter = false
    @State private var importKind: ImportKind = .csv
    @State private var activeDialog: ActiveDialog?
    @State private var showChartBuilder = false

    enum EditorTab: String, CaseIterable {
        case data = "Data View"
        case variable = "Variable View"
    }

    enum ImportKind { case csv, sav }
    enum ActiveDialog: Identifiable {
        case compute, recode, autoRecode, rank, sort, select, weight, split
        case frequencies, descriptives, explore, crosstabs, ttest, oneway, corr, regression
        case glm, factor, cluster, logistic, km, ctables, tree, extensions
        var id: String { String(describing: self) }
    }

    var body: some View {
        rootChrome
            .fileImporter(
                isPresented: $showImporter,
                allowedContentTypes: importKind == .csv ? [.commaSeparatedText, .plainText] : [.data],
                allowsMultipleSelection: false,
                onCompletion: handleImport
            )
            .sheet(item: $activeDialog, content: dialogHost)
            .sheet(isPresented: $showChartBuilder) { chartBuilderSheet }
            .dialogRouting(
                activeDialog: $activeDialog,
                showChartBuilder: $showChartBuilder,
                onNewDataset: { session.newDataset() }
            )
    }

    private var rootChrome: some View {
        VStack(spacing: 0) {
            toolbar
            Divider()
            editorArea
            statusBar
        }
    }

    @ViewBuilder
    private var editorArea: some View {
        if let ds = session.activeDataset {
            Group {
                switch editorTab {
                case .data: DataEditorView(dataset: ds)
                case .variable: VariableView(dataset: ds)
                }
            }
            .id(ds.id)
        } else {
            ContentUnavailableView("No Dataset", systemImage: "tablecells", description: Text("Create or open a dataset."))
        }
    }

    private var chartBuilderSheet: some View {
        ChartBuilderView()
            .environmentObject(session)
            .frame(minWidth: 720, minHeight: 480)
    }

    private var toolbar: some View {
        HStack(spacing: 12) {
            Text("StatDesk").font(.headline)
            Picker("", selection: $editorTab) {
                ForEach(EditorTab.allCases, id: \.self) { tab in
                    Text(tab.rawValue).tag(tab)
                }
            }
            .pickerStyle(.segmented)
            .frame(maxWidth: 280)

            Menu("File") {
                Button("New Dataset") { session.newDataset() }
                Divider()
                Button("Open CSV…") { importKind = .csv; showImporter = true }
                Button("Open SAV…") { importKind = .sav; showImporter = true }
                Button("Save CSV…") { saveCSV() }
                Button("Save SAV…") { saveSAV() }
            }

            if let ds = session.activeDataset {
                Button("Undo") { ds.undo() }
                Button("Redo") { ds.redo() }
            }

            Spacer()

            Picker("Dataset", selection: Binding(
                get: { session.activeDatasetID },
                set: { session.activeDatasetID = $0 }
            )) {
                ForEach(session.datasets) { ds in
                    Text(ds.displayName).tag(Optional(ds.id))
                }
            }
            .frame(maxWidth: 200)

            OpenWindowButton(title: "Syntax", id: "syntax")
            OpenWindowButton(title: "Output", id: "output")
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 8)
        .background(.bar)
    }

    private var statusBar: some View {
        HStack(spacing: 16) {
            Text(session.statusMessage)
            Spacer()
            if let ds = session.activeDataset {
                statusChip("Filter", on: ds.filterOn)
                statusChip("Weight", on: ds.weightOn)
                statusChip("Split File", on: ds.splitOn)
                Text("\(ds.caseCount) cases, \(ds.variableCount) variables")
                    .foregroundStyle(.secondary)
            }
        }
        .font(.caption)
        .padding(.horizontal, 12)
        .padding(.vertical, 6)
        .background(Color(nsColor: .windowBackgroundColor))
    }

    private func statusChip(_ title: String, on: Bool) -> some View {
        Text("\(title) \(on ? "On" : "Off")")
            .padding(.horizontal, 8)
            .padding(.vertical, 2)
            .background(on ? Color.accentColor.opacity(0.2) : Color.secondary.opacity(0.12))
            .clipShape(RoundedRectangle(cornerRadius: 4))
    }

    @ViewBuilder
    private func dialogHost(_ dialog: ActiveDialog) -> some View {
        switch dialog {
        case .compute: ComputeDialog(onClose: { activeDialog = nil })
        case .recode: RecodeDialog(onClose: { activeDialog = nil })
        case .autoRecode: AutoRecodeDialog(onClose: { activeDialog = nil })
        case .rank: RankDialog(onClose: { activeDialog = nil })
        case .sort: SortDialog(onClose: { activeDialog = nil })
        case .select: SelectCasesDialog(onClose: { activeDialog = nil })
        case .weight: WeightDialog(onClose: { activeDialog = nil })
        case .split: SplitFileDialog(onClose: { activeDialog = nil })
        case .frequencies: FrequenciesDialog(onClose: { activeDialog = nil })
        case .descriptives: DescriptivesDialog(onClose: { activeDialog = nil })
        case .explore: ExploreDialog(onClose: { activeDialog = nil })
        case .crosstabs: CrosstabsDialog(onClose: { activeDialog = nil })
        case .ttest: TTestDialog(onClose: { activeDialog = nil })
        case .oneway: OnewayDialog(onClose: { activeDialog = nil })
        case .corr: CorrelationsDialog(onClose: { activeDialog = nil })
        case .regression: RegressionDialog(onClose: { activeDialog = nil })
        case .glm: GLMDialog(onClose: { activeDialog = nil })
        case .factor: FactorDialog(onClose: { activeDialog = nil })
        case .cluster: ClusterDialog(onClose: { activeDialog = nil })
        case .logistic: LogisticDialog(onClose: { activeDialog = nil })
        case .km: KMDialog(onClose: { activeDialog = nil })
        case .ctables: CTablesDialog(onClose: { activeDialog = nil })
        case .tree: TreeDialog(onClose: { activeDialog = nil })
        case .extensions: ExtensionsStatusDialog(onClose: { activeDialog = nil })
        }
    }

    private func handleImport(_ result: Result<[URL], Error>) {
        do {
            guard let url = try result.get().first else { return }
            _ = url.startAccessingSecurityScopedResource()
            defer { url.stopAccessingSecurityScopedResource() }
            if importKind == .csv {
                try session.openCSV(url: url)
            } else {
                try session.openSAV(url: url)
            }
        } catch {
            session.statusMessage = error.localizedDescription
        }
    }

    private func saveCSV() {
        let panel = NSSavePanel()
        panel.allowedContentTypes = [.commaSeparatedText]
        panel.nameFieldStringValue = "dataset.csv"
        if panel.runModal() == .OK, let url = panel.url, let ds = session.activeDataset {
            try? ds.engine.writeCSV(path: url.path)
            session.statusMessage = "Saved \(url.lastPathComponent)"
        }
    }

    private func saveSAV() {
        let panel = NSSavePanel()
        panel.allowedContentTypes = [.data]
        panel.nameFieldStringValue = "dataset.sav"
        if panel.runModal() == .OK, let url = panel.url, let ds = session.activeDataset {
            try? ds.engine.writeSAV(path: url.path)
            session.statusMessage = "Saved \(url.lastPathComponent)"
        }
    }
}

struct OpenWindowButton: View {
    let title: String
    let id: String
    @Environment(\.openWindow) private var openWindow
    var body: some View {
        Button(title) { openWindow(id: id) }
    }
}

struct SettingsView: View {
    @EnvironmentObject private var capabilities: CapabilityRegistry
    var body: some View {
        Form {
            Text("Target: IBM SPSS Statistics 30.x parity reference")
            Text("Product: StatDesk — independent implementation, original branding")
            Text("Numerical/UI parity with SPSS is not claimed; see parity/matrices.")
            Section("Modules") {
                ForEach(ModuleEdition.allCases, id: \.self) { mod in
                    Toggle(mod.rawValue, isOn: Binding(
                        get: { capabilities.enabledModules.contains(mod) },
                        set: { on in
                            if on { capabilities.enabledModules.insert(mod) }
                            else if mod != .base { capabilities.enabledModules.remove(mod) }
                        }
                    ))
                    .disabled(mod != .base && !(capabilities.all.contains { $0.module == mod && $0.available }))
                }
            }
            Section("Extensions") {
                ForEach(ExtensionHost.status(), id: \.runtime) { cap in
                    LabeledContent(cap.runtime.rawValue, value: cap.available ? (cap.interpreterPath ?? "available") : cap.reason)
                }
            }
        }
        .padding()
        .frame(width: 480)
    }
}

struct ExtensionsStatusDialog: View {
    var onClose: () -> Void
    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Extension Runtimes").font(.headline)
            Text("BEGIN PROGRAM runs in an isolated subprocess when an interpreter is on PATH.")
                .foregroundStyle(.secondary)
            ForEach(ExtensionHost.status(), id: \.runtime) { cap in
                HStack {
                    Image(systemName: cap.available ? "checkmark.circle.fill" : "xmark.circle")
                        .foregroundStyle(cap.available ? .green : .secondary)
                    VStack(alignment: .leading) {
                        Text(cap.runtime.rawValue).font(.headline)
                        Text(cap.reason).font(.caption).foregroundStyle(.secondary)
                    }
                }
            }
            HStack {
                Spacer()
                Button("Close", action: onClose).keyboardShortcut(.cancelAction)
            }
        }
        .padding(20)
        .frame(width: 420)
    }
}
