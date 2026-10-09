// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "StatDesk",
    platforms: [.macOS(.v14)],
    products: [
        .library(name: "SPSSCore", targets: ["SPSSCore"]),
        .library(name: "SPSSSyntax", targets: ["SPSSSyntax"]),
        .executable(name: "statdesk-cli", targets: ["StatDeskCLI"]),
        .executable(name: "statdesk-smoke", targets: ["StatDeskSmoke"]),
        .executable(name: "StatDesk", targets: ["StatDeskApp"])
    ],
    targets: [
        .target(
            name: "SPSSEngineC",
            path: "engine",
            exclude: [
                "CMakeLists.txt",
                "build",
                "tests",
                "src/cli",
                "src/procedure/StatsUtil.hpp"
            ],
            publicHeadersPath: "include",
            cxxSettings: [
                .headerSearchPath("include"),
                .headerSearchPath("src/procedure"),
                .define("SPSS_ENGINE_SPM")
            ],
            linkerSettings: [
                .linkedLibrary("c++")
            ]
        ),
        .target(
            name: "SPSSCore",
            dependencies: ["SPSSEngineC"],
            path: "apps/SPSSMac/Services",
            sources: [
                "EngineBridge.swift",
                "DatasetModel.swift",
                "ProjectSession.swift",
                "CapabilityRegistry.swift",
                "AutosaveService.swift",
                "ExportService.swift",
                "ExtensionHost.swift"
            ]
        ),
        .target(
            name: "SPSSSyntax",
            dependencies: ["SPSSCore"],
            path: "syntax/Sources/SPSSSyntax"
        ),
        .executableTarget(
            name: "StatDeskCLI",
            dependencies: ["SPSSCore", "SPSSSyntax"],
            path: "apps/StatDeskCLI"
        ),
        .executableTarget(
            name: "StatDeskSmoke",
            dependencies: ["SPSSCore", "SPSSSyntax"],
            path: "apps/StatDeskSmoke"
        ),
        .executableTarget(
            name: "StatDeskApp",
            dependencies: ["SPSSCore", "SPSSSyntax"],
            path: "apps/SPSSMac",
            exclude: [
                "Info.plist",
                "Resources",
                "Services",
                "Bridging",
                "Models"
            ]
        ),
        .testTarget(
            name: "SPSSSyntaxTests",
            dependencies: ["SPSSSyntax", "SPSSCore"],
            path: "syntax/Tests/SPSSSyntaxTests"
        ),
        .testTarget(
            name: "SPSSCoreTests",
            dependencies: ["SPSSCore", "SPSSSyntax"],
            path: "parity/tests"
        )
    ],
    cxxLanguageStandard: .cxx17
)
