import XCTest
@testable import SPSSSyntax

final class ParserTests: XCTestCase {
    func testComputeParse() {
        let (units, diags) = SyntaxParser.parse("COMPUTE total = age + score.")
        XCTAssertTrue(diags.isEmpty)
        XCTAssertEqual(units.count, 1)
        if case .compute(let t, let e) = units[0].command {
            XCTAssertEqual(t, "total")
            XCTAssertEqual(e, "age + score")
        } else {
            XCTFail("expected compute")
        }
    }

    func testUnknownCommandDiagnostic() {
        let (_, diags) = SyntaxParser.parse("FOOBAR x = 1.")
        XCTAssertFalse(diags.isEmpty)
        XCTAssertTrue(diags[0].message.contains("Unknown"))
    }

    func testFrequenciesPaste() {
        let s = PasteSyntax.frequencies(["sex", "group"])
        XCTAssertTrue(s.contains("FREQUENCIES"))
        XCTAssertTrue(s.contains("sex"))
    }

    func testCrosstabsParse() {
        let (units, _) = SyntaxParser.parse("CROSSTABS /TABLES=sex BY group.")
        XCTAssertEqual(units.count, 1)
        if case .crosstabs(let r, let c) = units[0].command {
            XCTAssertTrue(r.contains("sex"))
            XCTAssertEqual(c, "group")
        } else {
            XCTFail("expected crosstabs")
        }
    }

    func testUnianovaParse() {
        let (units, diags) = SyntaxParser.parse("UNIANOVA score BY group WITH age.")
        XCTAssertTrue(diags.isEmpty)
        if case .unianova(let dep, let factors, let covs) = units[0].command {
            XCTAssertEqual(dep, "score")
            XCTAssertEqual(factors, ["group"])
            XCTAssertEqual(covs, ["age"])
        } else {
            XCTFail("expected unianova")
        }
    }

    func testCTablesPaste() {
        let s = PasteSyntax.ctables(row: "sex", column: "group")
        XCTAssertTrue(s.contains("CTABLES"))
        XCTAssertTrue(s.contains("sex"))
    }
}
