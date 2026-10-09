import Foundation
import AppKit

public enum ExportFormat: String, CaseIterable, Identifiable {
    case html, csv, png, pdf
    public var id: String { rawValue }
    public var title: String {
        switch self {
        case .html: return "HTML"
        case .csv: return "CSV (tables)"
        case .png: return "PNG (rendered HTML snapshot)"
        case .pdf: return "PDF (printed HTML)"
        }
    }
}

public enum ExportService {
    public static func exportHTML(_ items: [OutputSnapshot], to url: URL) throws {
        let body = items.map { "<section><h1>\($0.title)</h1>\($0.html)</section>" }.joined()
        let doc = """
        <!DOCTYPE html><html><head><meta charset="utf-8"><title>StatDesk Output</title></head><body>
        \(body)
        </body></html>
        """
        try doc.write(to: url, atomically: true, encoding: .utf8)
    }

    public static func exportCombinedHTML(_ htmlFragments: [String], to url: URL) throws {
        let doc = """
        <!DOCTYPE html><html><head><meta charset="utf-8"><title>StatDesk Output</title>
        <style>body{font-family:-apple-system,sans-serif;margin:24px}hr{margin:24px 0;border:none;border-top:1px solid #ccc}</style>
        </head><body>
        \(htmlFragments.joined(separator: "\n<hr/>\n"))
        </body></html>
        """
        try doc.write(to: url, atomically: true, encoding: .utf8)
    }

    /// Best-effort PDF via NSPrintOperation / HTML data. Not DOCX/XLSX parity.
    public static func exportPDF(html: String, to url: URL) throws {
        let full = html.contains("<html") ? html : "<html><body>\(html)</body></html>"
        guard let data = full.data(using: .utf8) else {
            throw EngineError.message("Could not encode HTML for PDF export")
        }
        let view = NSTextView(frame: NSRect(x: 0, y: 0, width: 612, height: 792))
        if let attr = try? NSAttributedString(
            data: data,
            options: [.documentType: NSAttributedString.DocumentType.html,
                      .characterEncoding: String.Encoding.utf8.rawValue],
            documentAttributes: nil
        ) {
            view.textStorage?.setAttributedString(attr)
        } else {
            view.string = full
        }
        let printInfo = NSPrintInfo.shared.copy() as! NSPrintInfo
        printInfo.paperSize = NSSize(width: 612, height: 792)
        printInfo.topMargin = 36
        printInfo.bottomMargin = 36
        printInfo.leftMargin = 36
        printInfo.rightMargin = 36
        printInfo.jobDisposition = .save
        printInfo.dictionary()[NSPrintInfo.AttributeKey.jobSavingURL] = url
        let op = NSPrintOperation(view: view, printInfo: printInfo)
        op.showsPrintPanel = false
        op.showsProgressPanel = false
        if !op.run() {
            // Fallback: write HTML beside requested PDF path
            let fallback = url.deletingPathExtension().appendingPathExtension("html")
            try full.write(to: fallback, atomically: true, encoding: .utf8)
            throw EngineError.message("PDF print failed; wrote HTML to \(fallback.lastPathComponent)")
        }
    }

    public static func exportPNG(html: String, to url: URL) throws {
        let full = html.contains("<html") ? html : "<html><body>\(html)</body></html>"
        guard let data = full.data(using: .utf8) else {
            throw EngineError.message("Could not encode HTML for PNG export")
        }
        let view = NSTextView(frame: NSRect(x: 0, y: 0, width: 800, height: 600))
        if let attr = try? NSAttributedString(
            data: data,
            options: [.documentType: NSAttributedString.DocumentType.html,
                      .characterEncoding: String.Encoding.utf8.rawValue],
            documentAttributes: nil
        ) {
            view.textStorage?.setAttributedString(attr)
        } else {
            view.string = full
        }
        view.layoutManager?.ensureLayout(for: view.textContainer!)
        let size = view.layoutManager?.usedRect(for: view.textContainer!).size ?? NSSize(width: 800, height: 600)
        view.frame = NSRect(origin: .zero, size: NSSize(width: max(800, size.width), height: max(600, size.height)))
        guard let rep = view.bitmapImageRepForCachingDisplay(in: view.bounds) else {
            throw EngineError.message("PNG render failed")
        }
        view.cacheDisplay(in: view.bounds, to: rep)
        guard let png = rep.representation(using: .png, properties: [:]) else {
            throw EngineError.message("PNG encode failed")
        }
        try png.write(to: url)
    }
}
