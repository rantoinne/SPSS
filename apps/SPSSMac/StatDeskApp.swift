import SwiftUI
import SPSSCore

@main
struct StatDeskApp: App {
    @StateObject private var session = ProjectSession()
    @StateObject private var capabilities = CapabilityRegistry()
    @StateObject private var autosave = AutosaveService()

    var body: some Scene {
        WindowGroup("StatDesk") {
            ContentView()
                .environmentObject(session)
                .environmentObject(capabilities)
                .environmentObject(autosave)
                .frame(minWidth: 1100, minHeight: 700)
                .onAppear { autosave.start(session: session) }
        }
        .commands {
            StatDeskCommands()
        }

        Window("Syntax Editor", id: "syntax") {
            SyntaxEditorView()
                .environmentObject(session)
                .frame(minWidth: 640, minHeight: 480)
        }

        Window("Output Viewer", id: "output") {
            OutputViewerView()
                .environmentObject(session)
                .frame(minWidth: 720, minHeight: 520)
        }

        Settings {
            SettingsView()
                .environmentObject(capabilities)
        }
    }
}
