import SwiftUI
import Combine
import SPSSCore

/// Collapses many menu notifications into one published action (avoids SwiftUI type-check blowups).
@MainActor
final class MenuActionBridge: ObservableObject {
    enum Action: Equatable {
        case newDataset
        case chartBuilder
        case dialog(ContentView.ActiveDialog)
    }

    @Published var action: Action?
    private var tokens: [NSObjectProtocol] = []

    init() {
        let pairs: [(Notification.Name, Action)] = [
            (.newDataset, .newDataset),
            (.showChartBuilder, .chartBuilder),
            (.showComputeDialog, .dialog(.compute)),
            (.showRecodeDialog, .dialog(.recode)),
            (.showAutoRecodeDialog, .dialog(.autoRecode)),
            (.showRankDialog, .dialog(.rank)),
            (.showSortDialog, .dialog(.sort)),
            (.showSelectDialog, .dialog(.select)),
            (.showWeightDialog, .dialog(.weight)),
            (.showSplitDialog, .dialog(.split)),
            (.showFrequenciesDialog, .dialog(.frequencies)),
            (.showDescriptivesDialog, .dialog(.descriptives)),
            (.showExploreDialog, .dialog(.explore)),
            (.showCrosstabsDialog, .dialog(.crosstabs)),
            (.showTTestDialog, .dialog(.ttest)),
            (.showOnewayDialog, .dialog(.oneway)),
            (.showCorrDialog, .dialog(.corr)),
            (.showRegressionDialog, .dialog(.regression)),
            (.showLogisticDialog, .dialog(.logistic)),
            (.showGLMDialog, .dialog(.glm)),
            (.showFactorDialog, .dialog(.factor)),
            (.showClusterDialog, .dialog(.cluster)),
            (.showKMDialog, .dialog(.km)),
            (.showCTablesDialog, .dialog(.ctables)),
            (.showTreeDialog, .dialog(.tree)),
            (.showExtensionsStatus, .dialog(.extensions))
        ]
        for (name, action) in pairs {
            let token = NotificationCenter.default.addObserver(forName: name, object: nil, queue: .main) { [weak self] _ in
                Task { @MainActor in self?.action = action }
            }
            tokens.append(token)
        }
    }

    deinit {
        for t in tokens { NotificationCenter.default.removeObserver(t) }
    }
}

struct DialogRoutingModifier: ViewModifier {
    @Binding var activeDialog: ContentView.ActiveDialog?
    @Binding var showChartBuilder: Bool
    let onNewDataset: () -> Void
    @StateObject private var bridge = MenuActionBridge()

    func body(content: Content) -> some View {
        content.onChange(of: bridge.action) { _, newValue in
            guard let newValue else { return }
            switch newValue {
            case .newDataset: onNewDataset()
            case .chartBuilder: showChartBuilder = true
            case .dialog(let d): activeDialog = d
            }
            bridge.action = nil
        }
    }
}

extension View {
    func dialogRouting(
        activeDialog: Binding<ContentView.ActiveDialog?>,
        showChartBuilder: Binding<Bool>,
        onNewDataset: @escaping () -> Void
    ) -> some View {
        modifier(DialogRoutingModifier(
            activeDialog: activeDialog,
            showChartBuilder: showChartBuilder,
            onNewDataset: onNewDataset
        ))
    }
}
