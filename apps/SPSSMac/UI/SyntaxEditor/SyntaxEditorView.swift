import SwiftUI
import SPSSCore
import SPSSSyntax

struct SyntaxEditorView: View {
    @EnvironmentObject private var session: ProjectSession
    @State private var text: String = "* StatDesk Syntax Editor.\n* Target: SPSS 30 Base command subset.\n\n"
    @State private var diagnostics: [SyntaxDiagnostic] = []
    @State private var log: [String] = []
    private let runtime = SyntaxRuntime()

    var body: some View {
        VStack(spacing: 0) {
            HStack {
                Text("Syntax Editor").font(.headline)
                Spacer()
                Button("Run All") { run(selectionOnly: false) }
                    .keyboardShortcut("r", modifiers: [.command])
                Button("Run Selection") { run(selectionOnly: true) }
            }
            .padding(8)
            Divider()
            HStack(spacing: 0) {
                TextEditor(text: $text)
                    .font(.system(.body, design: .monospaced))
                    .padding(4)
                Divider()
                VStack(alignment: .leading, spacing: 8) {
                    Text("Problems").font(.subheadline.bold())
                    if diagnostics.isEmpty {
                        Text("No diagnostics").foregroundStyle(.secondary)
                    } else {
                        List(diagnostics) { d in
                            Text("Line \(d.line): \(d.message)")
                                .foregroundStyle(d.isError ? .red : .primary)
                                .font(.caption)
                        }
                    }
                    Text("Log").font(.subheadline.bold())
                    ScrollView {
                        VStack(alignment: .leading) {
                            ForEach(Array(log.enumerated()), id: \.offset) { _, line in
                                Text(line).font(.system(.caption, design: .monospaced))
                            }
                        }
                        .frame(maxWidth: .infinity, alignment: .leading)
                    }
                }
                .frame(width: 280)
                .padding(8)
            }
        }
        .onAppear {
            if let id = session.activeSyntaxID,
               let doc = session.syntaxDocs.first(where: { $0.id == id }) {
                text = doc.text.isEmpty ? text : doc.text
            }
        }
        .onChange(of: text) { _, newValue in
            if let id = session.activeSyntaxID {
                session.updateSyntax(id: id, text: newValue)
            }
        }
    }

    private func run(selectionOnly: Bool) {
        let result = runtime.run(text: text, session: session, selectionOnly: false)
        diagnostics = result.diagnostics
        log = result.log
    }
}
