import XCTest
import SPSSCore

final class CoreIOTests: XCTestCase {
    func testCSVRoundTrip() throws {
        let ds = EngineDataset(name: "t")
        _ = try ds.addNumeric("x")
        _ = try ds.addNumeric("y")
        _ = try ds.addCases(3)
        ds.setNumeric(row: 0, col: 0, value: 1)
        ds.setNumeric(row: 1, col: 0, value: 2)
        ds.setNumeric(row: 2, col: 0, value: 3)
        ds.setNumeric(row: 0, col: 1, value: 10)
        ds.setNumeric(row: 1, col: 1, value: 20)
        ds.setNumeric(row: 2, col: 1, value: 30)

        let dir = FileManager.default.temporaryDirectory
        let csv = dir.appendingPathComponent("statdesk_test.csv")
        let sav = dir.appendingPathComponent("statdesk_test.sav")
        try ds.writeCSV(path: csv.path)
        try ds.writeSAV(path: sav.path)

        let loaded = try EngineDataset.readCSV(path: csv.path)
        XCTAssertEqual(loaded.caseCount, 3)
        XCTAssertEqual(loaded.variableCount, 2)

        let loadedSav = try EngineDataset.readSAV(path: sav.path)
        XCTAssertEqual(loadedSav.caseCount, 3)
        XCTAssertEqual(loadedSav.getNumeric(row: 1, col: 1), 20, accuracy: 1e-9)
    }

    func testDescriptivesSmoke() throws {
        let ds = EngineDataset(name: "t")
        _ = try ds.addNumeric("score")
        _ = try ds.addCases(5)
        for i in 0..<5 { ds.setNumeric(row: i, col: 0, value: Double(70 + i * 5)) }
        let out = ds.descriptives(["score"])
        XCTAssertFalse(out.html.isEmpty)
        XCTAssertTrue(out.html.contains("Descriptive") || out.html.contains("score"))
    }

    func testAdvancedProceduresSmoke() throws {
        let root = URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
        let sample = root.appendingPathComponent("parity/fixtures/sample.csv")
        guard FileManager.default.fileExists(atPath: sample.path) else {
            throw XCTSkip("sample.csv not found from cwd")
        }
        let ds = try EngineDataset.readCSV(path: sample.path)
        XCTAssertFalse(ds.glm(dependent: "score", factors: ["group"]).html.isEmpty)
        XCTAssertTrue(ds.factor(["age", "score"]).html.contains("Component") ||
                      ds.factor(["age", "score"]).html.contains("FACTOR"))
        XCTAssertTrue(ds.cluster(["age", "score"], k: 2).html.contains("Cluster"))
        XCTAssertTrue(ds.ctables(row: "sex", column: "group").html.contains("Custom") ||
                      ds.ctables(row: "sex", column: "group").html.contains("Table"))
        XCTAssertTrue(ds.exact(row: "sex", column: "group").html.contains("Fisher"))
    }

    func testExtensionHostProbe() {
        let status = ExtensionHost.status()
        XCTAssertEqual(status.count, 2)
    }
}
