import Foundation

@MainActor
public final class AutosaveService: ObservableObject {
    public let directory: URL
    private var timer: Timer?

    public init() {
        let base = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first!
        directory = base.appendingPathComponent("StatDesk/Autosave", isDirectory: true)
        try? FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
    }

    public func start(session: ProjectSession, interval: TimeInterval = 60) {
        timer?.invalidate()
        timer = Timer.scheduledTimer(withTimeInterval: interval, repeats: true) { [weak self] _ in
            Task { @MainActor in
                self?.checkpoint(session: session)
            }
        }
    }

    public func stop() {
        timer?.invalidate()
        timer = nil
    }

    public func checkpoint(session: ProjectSession) {
        guard let ds = session.activeDataset else { return }
        let url = directory.appendingPathComponent("active.sav")
        try? ds.engine.writeSAV(path: url.path)
        let html = session.outputItems.map(\.html).joined(separator: "\n<hr/>\n")
        let outURL = directory.appendingPathComponent("output.html")
        try? html.write(to: outURL, atomically: true, encoding: .utf8)
        session.statusMessage = "Autosaved"
    }
}
