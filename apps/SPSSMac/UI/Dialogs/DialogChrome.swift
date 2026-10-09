import SwiftUI

/// SPSS dialog button order: OK · Paste · Reset · Cancel · Help
struct DialogButtons: View {
    var onOK: () -> Void
    var onPaste: () -> Void
    var onReset: () -> Void
    var onCancel: () -> Void
    var onHelp: () -> Void
    var okEnabled: Bool = true

    var body: some View {
        HStack {
            Button("Help") { onHelp() }
            Spacer()
            Button("OK") { onOK() }
                .keyboardShortcut(.defaultAction)
                .disabled(!okEnabled)
            Button("Paste") { onPaste() }
            Button("Reset") { onReset() }
            Button("Cancel") { onCancel() }
                .keyboardShortcut(.cancelAction)
        }
        .padding(.top, 8)
    }
}

struct DialogShell<Content: View>: View {
    let title: String
    private let content: () -> Content
    var onOK: () -> Void
    var onPaste: () -> Void
    var onReset: () -> Void
    var onCancel: () -> Void
    var okEnabled: Bool

    init(
        title: String,
        onOK: @escaping () -> Void,
        onPaste: @escaping () -> Void,
        onReset: @escaping () -> Void,
        onCancel: @escaping () -> Void,
        okEnabled: Bool = true,
        @ViewBuilder content: @escaping () -> Content
    ) {
        self.title = title
        self.onOK = onOK
        self.onPaste = onPaste
        self.onReset = onReset
        self.onCancel = onCancel
        self.okEnabled = okEnabled
        self.content = content
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text(title).font(.headline)
            content()
            DialogButtons(onOK: onOK, onPaste: onPaste, onReset: onReset, onCancel: onCancel, onHelp: {
                // Help topics — original StatDesk docs, not IBM
            }, okEnabled: okEnabled)
        }
        .padding(20)
        .frame(minWidth: 480)
    }
}
