import SwiftUI
import WebKit
import SPSSCore

struct OutputViewerView: View {
    @EnvironmentObject private var session: ProjectSession
    @State private var selectedID: OutputSnapshot.ID?
    @State private var showExporter = false

    var body: some View {
        NavigationSplitView {
            List(session.outputItems, selection: $selectedID) { item in
                VStack(alignment: .leading) {
                    Text(item.title).font(.headline)
                    Text(item.createdAt.formatted(date: .omitted, time: .shortened))
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
                .tag(item.id)
            }
            .navigationTitle("Output")
            .toolbar {
                Button("Export HTML…") { exportHTML() }
            }
        } detail: {
            if let item = session.outputItems.first(where: { $0.id == selectedID }) ?? session.outputItems.first {
                HTMLView(html: item.html)
            } else {
                ContentUnavailableView("No Output", systemImage: "doc.text", description: Text("Run Analyze procedures or syntax to populate the Output Viewer."))
            }
        }
        .onAppear { selectedID = session.outputItems.first?.id }
        .onChange(of: session.outputItems.count) { _, _ in
            selectedID = session.outputItems.first?.id
        }
    }

    private func exportHTML() {
        let panel = NSSavePanel()
        panel.allowedContentTypes = [.html]
        panel.nameFieldStringValue = "output.html"
        if panel.runModal() == .OK, let url = panel.url {
            try? ExportService.exportHTML(session.outputItems, to: url)
            session.statusMessage = "Exported \(url.lastPathComponent)"
        }
    }
}

struct HTMLView: NSViewRepresentable {
    let html: String
    func makeNSView(context: Context) -> WKWebView {
        WKWebView()
    }
    func updateNSView(_ nsView: WKWebView, context: Context) {
        nsView.loadHTMLString(html, baseURL: nil)
    }
}
