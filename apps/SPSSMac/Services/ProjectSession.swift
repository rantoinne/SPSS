import Foundation
import Combine

public struct SyntaxDocument: Identifiable, Hashable {
    public let id = UUID()
    public var title: String
    public var text: String
    public init(title: String = "Syntax1", text: String = "") {
        self.title = title
        self.text = text
    }
}

public struct OutputSnapshot: Identifiable {
    public let id = UUID()
    public var title: String
    public var html: String
    public var createdAt: Date = Date()
}

@MainActor
public final class ProjectSession: ObservableObject {
    @Published public var datasets: [DatasetDocument] = []
    @Published public var activeDatasetID: DatasetDocument.ID?
    @Published public var syntaxDocs: [SyntaxDocument] = [SyntaxDocument()]
    @Published public var activeSyntaxID: SyntaxDocument.ID?
    @Published public var outputItems: [OutputSnapshot] = []
    @Published public var commandHistory: [String] = []
    @Published public var statusMessage: String = "Ready"
    @Published public var isRunning: Bool = false

    public init() {
        let ds = DatasetDocument.empty()
        datasets = [ds]
        activeDatasetID = ds.id
        activeSyntaxID = syntaxDocs.first?.id
    }

    public var activeDataset: DatasetDocument? {
        datasets.first { $0.id == activeDatasetID } ?? datasets.first
    }

    public func appendOutput(title: String, html: String) {
        outputItems.insert(OutputSnapshot(title: title, html: html), at: 0)
    }

    public func openCSV(url: URL) throws {
        let eng = try EngineDataset.readCSV(path: url.path)
        let doc = DatasetDocument(engine: eng, displayName: url.lastPathComponent)
        datasets.append(doc)
        activeDatasetID = doc.id
        statusMessage = "Opened \(url.lastPathComponent)"
    }

    public func openSAV(url: URL) throws {
        let eng = try EngineDataset.readSAV(path: url.path)
        let doc = DatasetDocument(engine: eng, displayName: url.lastPathComponent)
        datasets.append(doc)
        activeDatasetID = doc.id
        statusMessage = "Opened \(url.lastPathComponent)"
    }

    public func saveActiveCSV(url: URL) throws {
        guard let ds = activeDataset else { return }
        try ds.engine.writeCSV(path: url.path)
        statusMessage = "Saved \(url.lastPathComponent)"
    }

    public func saveActiveSAV(url: URL) throws {
        guard let ds = activeDataset else { return }
        try ds.engine.writeSAV(path: url.path)
        statusMessage = "Saved \(url.lastPathComponent)"
    }

    public func newDataset() {
        let name = "Dataset\(datasets.count + 1)"
        let doc = DatasetDocument.empty(name: name)
        datasets.append(doc)
        activeDatasetID = doc.id
    }

    public func pasteSyntax(_ text: String) {
        guard let idx = syntaxDocs.firstIndex(where: { $0.id == activeSyntaxID }) else {
            let doc = SyntaxDocument(text: text + "\n")
            syntaxDocs.append(doc)
            activeSyntaxID = doc.id
            return
        }
        if !syntaxDocs[idx].text.isEmpty && !syntaxDocs[idx].text.hasSuffix("\n") {
            syntaxDocs[idx].text += "\n"
        }
        syntaxDocs[idx].text += text
        if !text.hasSuffix("\n") { syntaxDocs[idx].text += "\n" }
    }

    public func updateSyntax(id: SyntaxDocument.ID, text: String) {
        guard let idx = syntaxDocs.firstIndex(where: { $0.id == id }) else { return }
        syntaxDocs[idx].text = text
    }
}
