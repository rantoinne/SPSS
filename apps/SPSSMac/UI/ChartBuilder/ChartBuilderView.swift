import SwiftUI
import AppKit
import Charts
import SPSSCore
import SPSSSyntax

private enum ChartPalette: String, CaseIterable, Identifiable {
    case vibrant = "Vibrant"
    case soft = "Soft"
    case earth = "Earth"
    case mono = "Mono Blue"

    var id: String { rawValue }

    var colors: [Color] {
        switch self {
        case .vibrant:
            return [
                Color(red: 0.12, green: 0.45, blue: 0.90),
                Color(red: 0.95, green: 0.35, blue: 0.25),
                Color(red: 0.20, green: 0.72, blue: 0.45),
                Color(red: 0.95, green: 0.65, blue: 0.15),
                Color(red: 0.55, green: 0.35, blue: 0.85),
                Color(red: 0.15, green: 0.70, blue: 0.80),
                Color(red: 0.90, green: 0.30, blue: 0.55),
                Color(red: 0.40, green: 0.55, blue: 0.20)
            ]
        case .soft:
            return [
                Color(red: 0.45, green: 0.65, blue: 0.90),
                Color(red: 0.95, green: 0.60, blue: 0.55),
                Color(red: 0.55, green: 0.82, blue: 0.65),
                Color(red: 0.95, green: 0.80, blue: 0.45),
                Color(red: 0.75, green: 0.60, blue: 0.90),
                Color(red: 0.55, green: 0.82, blue: 0.88),
                Color(red: 0.90, green: 0.60, blue: 0.75),
                Color(red: 0.70, green: 0.75, blue: 0.55)
            ]
        case .earth:
            return [
                Color(red: 0.55, green: 0.35, blue: 0.20),
                Color(red: 0.75, green: 0.50, blue: 0.25),
                Color(red: 0.40, green: 0.55, blue: 0.35),
                Color(red: 0.70, green: 0.55, blue: 0.30),
                Color(red: 0.45, green: 0.40, blue: 0.35),
                Color(red: 0.60, green: 0.45, blue: 0.50),
                Color(red: 0.35, green: 0.50, blue: 0.50),
                Color(red: 0.80, green: 0.65, blue: 0.40)
            ]
        case .mono:
            return [
                Color(red: 0.10, green: 0.30, blue: 0.70),
                Color(red: 0.20, green: 0.45, blue: 0.85),
                Color(red: 0.35, green: 0.55, blue: 0.90),
                Color(red: 0.50, green: 0.65, blue: 0.92),
                Color(red: 0.15, green: 0.35, blue: 0.60),
                Color(red: 0.25, green: 0.50, blue: 0.75),
                Color(red: 0.40, green: 0.60, blue: 0.80),
                Color(red: 0.55, green: 0.70, blue: 0.85)
            ]
        }
    }
}

struct ChartBuilderView: View {
    @EnvironmentObject private var session: ProjectSession
    @Environment(\.dismiss) private var dismiss
    @State private var chartType = "Bar"
    @State private var categoryVar = ""
    @State private var valueVar = ""
    @State private var palette: ChartPalette = .vibrant

    private let gallery = ["Bar", "Pie", "Histogram", "Scatter", "Box", "Line"]

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Text("Chart Builder").font(.title2.bold())
            Text("Gallery + drop-zone mental model (SPSS-like). Charts bind to the active dataset.")
                .foregroundStyle(.secondary)
            HStack(alignment: .top, spacing: 20) {
                VStack(alignment: .leading) {
                    Text("Gallery").font(.headline)
                    ForEach(gallery, id: \.self) { g in
                        Button(g) { chartType = g }
                            .buttonStyle(.bordered)
                            .tint(chartType == g ? .accentColor : .gray)
                    }
                }
                .frame(width: 140, alignment: .leading)

                VStack(alignment: .leading, spacing: 12) {
                    Text("Drop zones").font(.headline)
                    LabeledContent("Category / X") {
                        Picker("", selection: $categoryVar) {
                            Text("—").tag("")
                            ForEach(vars, id: \.self) { name in
                                Text(shortLabel(for: name)).tag(name)
                            }
                        }
                    }
                    if chartType != "Pie" {
                        LabeledContent("Value / Y") {
                            Picker("", selection: $valueVar) {
                                Text("— (count)").tag("")
                                ForEach(vars, id: \.self) { name in
                                    Text(shortLabel(for: name)).tag(name)
                                }
                            }
                        }
                    }
                    LabeledContent("Palette") {
                        Picker("", selection: $palette) {
                            ForEach(ChartPalette.allCases) { p in
                                Text(p.rawValue).tag(p)
                            }
                        }
                        .pickerStyle(.menu)
                    }
                    paletteSwatch
                    preview
                }
            }
            HStack {
                Spacer()
                DialogButtons(
                    onOK: { build(); dismiss() },
                    onPaste: { session.pasteSyntax(PasteSyntax.frequencies([categoryVar].filter { !$0.isEmpty })) },
                    onReset: {
                        categoryVar = ""
                        valueVar = ""
                        chartType = "Bar"
                        palette = .vibrant
                    },
                    onCancel: { dismiss() },
                    onHelp: {}
                )
            }
        }
        .padding(20)
        .frame(minWidth: 760, minHeight: 520)
        .onAppear {
            categoryVar = vars.first ?? ""
            valueVar = ""
        }
    }

    private var vars: [String] {
        session.activeDataset?.variableNames() ?? []
    }

    private func shortLabel(for name: String) -> String {
        guard let ds = session.activeDataset else { return name }
        let names = ds.variableNames()
        guard let idx = names.firstIndex(of: name) else { return name }
        let label = ds.variablesMeta()[idx].label
        if label.isEmpty { return name }
        if label.count <= 48 { return "\(name) — \(label)" }
        return "\(name) — \(label.prefix(45))…"
    }

    private var paletteSwatch: some View {
        HStack(spacing: 4) {
            ForEach(Array(palette.colors.enumerated()), id: \.offset) { _, color in
                RoundedRectangle(cornerRadius: 3)
                    .fill(color)
                    .frame(width: 18, height: 12)
            }
            Spacer()
        }
    }

    @ViewBuilder
    private var preview: some View {
        let points = chartPoints()
        let cats = points.map(\.label)
        Group {
            if points.isEmpty {
                Text("Select a category variable to preview.")
                    .foregroundStyle(.secondary)
                    .frame(maxWidth: .infinity, minHeight: 220)
            } else if chartType == "Pie" {
                Chart(points) { p in
                    SectorMark(
                        angle: .value("Value", p.y),
                        innerRadius: .ratio(0.08),
                        angularInset: 1.2
                    )
                    .foregroundStyle(by: .value("Category", p.label))
                    .annotation(position: .overlay) {
                        if points.count <= 8 {
                            Text(p.label.count > 12 ? String(p.label.prefix(10)) + "…" : p.label)
                                .font(.caption2)
                                .foregroundStyle(.primary)
                        }
                    }
                }
                .chartForegroundStyleScale(domain: cats, range: colorRange(for: cats.count))
                .chartLegend(position: .bottom, spacing: 8)
                .frame(height: 260)
            } else if chartType == "Line" {
                Chart(points) { p in
                    LineMark(x: .value("x", p.x), y: .value("y", p.y))
                        .foregroundStyle(palette.colors[0])
                    PointMark(x: .value("x", p.x), y: .value("y", p.y))
                        .foregroundStyle(palette.colors[1 % palette.colors.count])
                }
                .frame(height: 220)
            } else if chartType == "Scatter" {
                Chart(points) { p in
                    PointMark(x: .value("x", p.x), y: .value("y", p.y))
                        .foregroundStyle(by: .value("Category", p.label))
                }
                .chartForegroundStyleScale(domain: cats, range: colorRange(for: cats.count))
                .frame(height: 220)
            } else {
                // Bar / Histogram / Box — colored bars by category
                Chart(points) { p in
                    BarMark(x: .value("x", p.label), y: .value("y", p.y))
                        .foregroundStyle(by: .value("Category", p.label))
                }
                .chartForegroundStyleScale(domain: cats, range: colorRange(for: cats.count))
                .chartLegend(position: .bottom, spacing: 8)
                .frame(height: 220)
            }
        }
        .padding()
        .background(Color.secondary.opacity(0.08))
        .clipShape(RoundedRectangle(cornerRadius: 8))
    }

    private func colorRange(for count: Int) -> [Color] {
        guard count > 0 else { return palette.colors }
        var colors: [Color] = []
        for i in 0..<count {
            colors.append(palette.colors[i % palette.colors.count])
        }
        return colors
    }

    private struct Pt: Identifiable {
        let id = UUID()
        var label: String
        var x: Double
        var y: Double
    }

    /// Aggregate frequencies for categorical charts; per-case for scatter/line.
    private func chartPoints() -> [Pt] {
        guard let ds = session.activeDataset, !categoryVar.isEmpty else { return [] }
        let names = ds.variableNames()
        guard let cIdx = names.firstIndex(of: categoryVar) else { return [] }

        if chartType == "Scatter" || chartType == "Line" {
            var pts: [Pt] = []
            let n = min(ds.caseCount, 200)
            let vIdx = names.firstIndex(of: valueVar)
            for r in 0..<n {
                let label = ds.cellDisplay(row: r, col: cIdx)
                let y: Double
                if let vIdx, valueVar != categoryVar {
                    y = ds.engine.getNumeric(row: r, col: vIdx)
                } else {
                    y = Double(r + 1)
                }
                if y.isNaN { continue }
                pts.append(Pt(label: label.isEmpty ? "(blank)" : label, x: Double(r), y: y))
            }
            return pts
        }

        // Pie / Bar / Histogram / Box: count (or sum numeric Y) by category
        var totals: [String: Double] = [:]
        var order: [String] = []
        let vIdx = (!valueVar.isEmpty && valueVar != categoryVar) ? names.firstIndex(of: valueVar) : nil
        for r in 0..<ds.caseCount {
            var label = ds.cellDisplay(row: r, col: cIdx)
            if label.isEmpty { label = "(blank)" }
            let contrib: Double
            if let vIdx {
                let y = ds.engine.getNumeric(row: r, col: vIdx)
                if y.isNaN { continue }
                contrib = y
            } else {
                contrib = 1
            }
            if totals[label] == nil { order.append(label) }
            totals[label, default: 0] += contrib
        }
        return order.prefix(24).map { Pt(label: $0, x: 0, y: totals[$0] ?? 0) }
    }

    private func build() {
        guard let ds = session.activeDataset, !categoryVar.isEmpty else { return }
        let points = chartPoints()
        let title = "Chart Builder: \(chartType) — \(categoryVar)"
        var html = """
        <!DOCTYPE html><html><head><meta charset="utf-8"><title>\(escapeHTML(title))</title>
        <style>
        body{font-family:-apple-system,sans-serif;margin:24px}
        table{border-collapse:collapse;margin:12px 0}
        th,td{border:1px solid #ccc;padding:6px 10px}
        th{background:#f0f0f0}
        .swatch{display:inline-block;width:12px;height:12px;margin-right:6px;border-radius:2px;vertical-align:middle}
        </style></head><body>
        <h2>\(escapeHTML(title))</h2>
        <p>Palette: \(escapeHTML(palette.rawValue)) · N categories: \(points.count)</p>
        <table><tr><th></th><th>Category</th><th>Value</th></tr>
        """
        let colors = colorRange(for: points.count)
        for (i, p) in points.enumerated() {
            let hex = cssHex(colors[i % colors.count])
            html += "<tr><td><span class=\"swatch\" style=\"background:\(hex)\"></span></td>"
            html += "<td>\(escapeHTML(p.label))</td><td>\(formatNum(p.y))</td></tr>\n"
        }
        html += "</table></body></html>"
        session.appendOutput(title: title, html: html)
        // Also attach frequencies table for the category
        let out = ds.engine.frequencies([categoryVar])
        session.appendOutput(title: "Frequencies: \(categoryVar)", html: out.html)
        session.statusMessage = "\(chartType) chart added to Output"
    }

    private func escapeHTML(_ s: String) -> String {
        s.replacingOccurrences(of: "&", with: "&amp;")
            .replacingOccurrences(of: "<", with: "&lt;")
            .replacingOccurrences(of: ">", with: "&gt;")
            .replacingOccurrences(of: "\"", with: "&quot;")
    }

    private func formatNum(_ v: Double) -> String {
        if v.rounded() == v { return String(Int(v)) }
        return String(format: "%.3f", v)
    }

    private func cssHex(_ color: Color) -> String {
        let ns = NSColor(color)
        guard let rgb = ns.usingColorSpace(.sRGB) else { return "#4A90D9" }
        let r = Int((rgb.redComponent * 255).rounded())
        let g = Int((rgb.greenComponent * 255).rounded())
        let b = Int((rgb.blueComponent * 255).rounded())
        return String(format: "#%02X%02X%02X", r, g, b)
    }
}
