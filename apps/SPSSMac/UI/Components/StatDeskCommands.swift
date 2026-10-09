import SwiftUI
import SPSSCore

struct StatDeskCommands: Commands {
    var body: some Commands {
        CommandGroup(replacing: .newItem) {
            Button("New Dataset") {
                NotificationCenter.default.post(name: .newDataset, object: nil)
            }
            .keyboardShortcut("n", modifiers: [.command])
        }
        CommandMenu("Data") {
            Button("Sort Cases…") { NotificationCenter.default.post(name: .showSortDialog, object: nil) }
            Button("Select Cases…") { NotificationCenter.default.post(name: .showSelectDialog, object: nil) }
            Button("Weight Cases…") { NotificationCenter.default.post(name: .showWeightDialog, object: nil) }
            Button("Split File…") { NotificationCenter.default.post(name: .showSplitDialog, object: nil) }
        }
        CommandMenu("Transform") {
            Button("Compute Variable…") { NotificationCenter.default.post(name: .showComputeDialog, object: nil) }
            Button("Recode Into Different Variables…") { NotificationCenter.default.post(name: .showRecodeDialog, object: nil) }
            Button("Automatic Recode…") { NotificationCenter.default.post(name: .showAutoRecodeDialog, object: nil) }
            Button("Rank Cases…") { NotificationCenter.default.post(name: .showRankDialog, object: nil) }
        }
        CommandMenu("Analyze") {
            Button("Frequencies…") { NotificationCenter.default.post(name: .showFrequenciesDialog, object: nil) }
            Button("Descriptives…") { NotificationCenter.default.post(name: .showDescriptivesDialog, object: nil) }
            Button("Explore…") { NotificationCenter.default.post(name: .showExploreDialog, object: nil) }
            Button("Crosstabs…") { NotificationCenter.default.post(name: .showCrosstabsDialog, object: nil) }
            Divider()
            Button("One-Sample T Test…") { NotificationCenter.default.post(name: .showTTestDialog, object: nil) }
            Button("One-Way ANOVA…") { NotificationCenter.default.post(name: .showOnewayDialog, object: nil) }
            Button("Bivariate Correlations…") { NotificationCenter.default.post(name: .showCorrDialog, object: nil) }
            Button("Linear Regression…") { NotificationCenter.default.post(name: .showRegressionDialog, object: nil) }
            Button("Binary Logistic…") { NotificationCenter.default.post(name: .showLogisticDialog, object: nil) }
            Divider()
            Button("Univariate GLM…") { NotificationCenter.default.post(name: .showGLMDialog, object: nil) }
            Button("Factor (PCA)…") { NotificationCenter.default.post(name: .showFactorDialog, object: nil) }
            Button("K-Means Cluster…") { NotificationCenter.default.post(name: .showClusterDialog, object: nil) }
            Button("Kaplan-Meier…") { NotificationCenter.default.post(name: .showKMDialog, object: nil) }
            Button("Custom Tables…") { NotificationCenter.default.post(name: .showCTablesDialog, object: nil) }
            Button("Decision Tree…") { NotificationCenter.default.post(name: .showTreeDialog, object: nil) }
        }
        CommandMenu("Graphs") {
            Button("Chart Builder…") { NotificationCenter.default.post(name: .showChartBuilder, object: nil) }
        }
        CommandMenu("Extensions") {
            Button("Python / R Status…") { NotificationCenter.default.post(name: .showExtensionsStatus, object: nil) }
        }
    }
}

extension Notification.Name {
    static let newDataset = Notification.Name("newDataset")
    static let showComputeDialog = Notification.Name("showComputeDialog")
    static let showRecodeDialog = Notification.Name("showRecodeDialog")
    static let showAutoRecodeDialog = Notification.Name("showAutoRecodeDialog")
    static let showRankDialog = Notification.Name("showRankDialog")
    static let showSortDialog = Notification.Name("showSortDialog")
    static let showSelectDialog = Notification.Name("showSelectDialog")
    static let showWeightDialog = Notification.Name("showWeightDialog")
    static let showSplitDialog = Notification.Name("showSplitDialog")
    static let showFrequenciesDialog = Notification.Name("showFrequenciesDialog")
    static let showDescriptivesDialog = Notification.Name("showDescriptivesDialog")
    static let showExploreDialog = Notification.Name("showExploreDialog")
    static let showCrosstabsDialog = Notification.Name("showCrosstabsDialog")
    static let showTTestDialog = Notification.Name("showTTestDialog")
    static let showOnewayDialog = Notification.Name("showOnewayDialog")
    static let showCorrDialog = Notification.Name("showCorrDialog")
    static let showRegressionDialog = Notification.Name("showRegressionDialog")
    static let showLogisticDialog = Notification.Name("showLogisticDialog")
    static let showGLMDialog = Notification.Name("showGLMDialog")
    static let showFactorDialog = Notification.Name("showFactorDialog")
    static let showClusterDialog = Notification.Name("showClusterDialog")
    static let showKMDialog = Notification.Name("showKMDialog")
    static let showCTablesDialog = Notification.Name("showCTablesDialog")
    static let showTreeDialog = Notification.Name("showTreeDialog")
    static let showChartBuilder = Notification.Name("showChartBuilder")
    static let showExtensionsStatus = Notification.Name("showExtensionsStatus")
}
