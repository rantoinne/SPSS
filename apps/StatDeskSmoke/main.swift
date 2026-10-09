import Foundation
import SPSSCore
import SPSSSyntax

@main
enum StatDeskSmoke {
    static var failures = 0

    static func check(_ cond: @autoclosure () -> Bool, _ msg: String) {
        if !cond() {
            fputs("FAIL: \(msg)\n", stderr)
            failures += 1
        } else {
            print("ok: \(msg)")
        }
    }

    static func main() {
        do {
            try run()
        } catch {
            fputs("ERROR: \(error)\n", stderr)
            exit(1)
        }
        if failures > 0 {
            fputs("\(failures) failure(s)\n", stderr)
            exit(1)
        }
        print("All Swift smoke checks passed")
    }

    static func run() throws {
        // Parser
        let (units, diags) = SyntaxParser.parse("COMPUTE total = age + score.")
        check(diags.isEmpty, "compute has no diagnostics")
        check(units.count == 1, "one compute unit")
        if case .compute(let t, let e) = units.first?.command {
            check(t == "total", "compute target")
            check(e == "age + score", "compute expression")
        } else {
            check(false, "compute command shape")
        }

        let (_, unknownDiags) = SyntaxParser.parse("FOOBAR x = 1.")
        check(!unknownDiags.isEmpty && unknownDiags[0].message.contains("Unknown"), "unknown command diagnostic")

        let paste = PasteSyntax.frequencies(["sex", "group"])
        check(paste.contains("FREQUENCIES") && paste.contains("sex"), "paste frequencies")

        let (freqUnits, _) = SyntaxParser.parse("FREQUENCIES VARIABLES=sex group.")
        if case .frequencies(let vars) = freqUnits.first?.command {
            check(vars == ["sex", "group"], "frequencies VARIABLES= parse")
        } else {
            check(false, "frequencies VARIABLES= parse")
        }

        let (ctUnits, _) = SyntaxParser.parse("CROSSTABS /TABLES=sex BY group.")
        if case .crosstabs(let r, let c) = ctUnits.first?.command {
            check(r == "sex" && c == "group", "crosstabs TABLES= parse")
        } else {
            check(false, "crosstabs TABLES= parse")
        }

        // I/O + procedures via C bridge
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
        let csv = dir.appendingPathComponent("statdesk_smoke.csv")
        let sav = dir.appendingPathComponent("statdesk_smoke.sav")
        try ds.writeCSV(path: csv.path)
        try ds.writeSAV(path: sav.path)
        let loaded = try EngineDataset.readCSV(path: csv.path)
        check(loaded.caseCount == 3 && loaded.variableCount == 2, "csv round-trip")
        let loadedSav = try EngineDataset.readSAV(path: sav.path)
        check(abs(loadedSav.getNumeric(row: 1, col: 1) - 20) < 1e-9, "sav round-trip")

        let desc = ds.descriptives(["x", "y"])
        check(!desc.html.isEmpty, "descriptives html")

        try ds.compute(target: "z", expression: "x + y")
        check(abs(ds.getNumeric(row: 0, col: 2) - 11) < 1e-9, "compute x+y")

        let (glmUnits, _) = SyntaxParser.parse("UNIANOVA y BY x.")
        if case .unianova(let dep, let factors, _) = glmUnits.first?.command {
            check(dep == "y" && factors == ["x"], "unianova parse")
        } else {
            check(false, "unianova parse")
        }

        let factorOut = ds.factor(["x", "y"], nFactors: 1)
        check(factorOut.html.contains("Component") || factorOut.html.contains("FACTOR"), "factor pca")

        let ext = ExtensionHost.status()
        check(!ext.isEmpty, "extension host status")

        // Fixture sample if present
        let root = URL(fileURLWithPath: FileManager.default.currentDirectoryPath)
        let sample = root.appendingPathComponent("parity/fixtures/sample.csv")
        if FileManager.default.fileExists(atPath: sample.path) {
            let s = try EngineDataset.readCSV(path: sample.path)
            check(s.caseCount == 6, "sample.csv cases")
            let html = s.descriptives(["age"]).html
            check(html.contains("age") || html.contains("Descriptive"), "sample descriptives")
            let glm = s.glm(dependent: "score", factors: ["group"], covariates: ["age"])
            check(glm.html.contains("Parameter") || glm.html.contains("Between"), "sample glm")
            let ct = s.ctables(row: "sex", column: "group")
            check(ct.html.contains("Custom Table") || ct.html.contains("CTABLES"), "sample ctables")
        }
    }
}
