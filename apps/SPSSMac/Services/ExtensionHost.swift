import Foundation

/// Phase 8 extension surface: discover interpreters and run isolated BEGIN PROGRAM blocks.
public enum ExtensionRuntime: String, CaseIterable, Sendable {
    case python = "Python"
    case r = "R"
}

public struct ExtensionCapability: Sendable {
    public var runtime: ExtensionRuntime
    public var available: Bool
    public var interpreterPath: String?
    public var reason: String
}

public struct ExtensionRunResult: Sendable {
    public var succeeded: Bool
    public var stdout: String
    public var stderr: String
    public var exitCode: Int32
}

/// Real process-isolation hooks. Interpreters are optional — unavailable → clear reason, no crash.
public enum ExtensionHost {
    public static func status() -> [ExtensionCapability] {
        [
            probe(.python, candidates: ["python3", "python"],
                  missing: "Install Python 3 to enable BEGIN PROGRAM-PYTHON"),
            probe(.r, candidates: ["Rscript"],
                  missing: "Install R (Rscript on PATH) to enable BEGIN PROGRAM-R")
        ]
    }

    public static func isAvailable(_ runtime: ExtensionRuntime) -> Bool {
        status().first(where: { $0.runtime == runtime })?.available == true
    }

    /// Runs `code` via a temporary file in an isolated subprocess (no shared address space with StatDesk).
    public static func run(runtime: ExtensionRuntime, code: String, workingDirectory: String? = nil) -> ExtensionRunResult {
        guard let cap = status().first(where: { $0.runtime == runtime }), cap.available, let exe = cap.interpreterPath else {
            return ExtensionRunResult(succeeded: false, stdout: "", stderr: "Interpreter not available for \(runtime.rawValue)", exitCode: 127)
        }
        let tmp = FileManager.default.temporaryDirectory
            .appendingPathComponent("statdesk-\(runtime.rawValue.lowercased())-\(UUID().uuidString)")
            .appendingPathExtension(runtime == .python ? "py" : "R")
        do {
            try code.write(to: tmp, atomically: true, encoding: .utf8)
        } catch {
            return ExtensionRunResult(succeeded: false, stdout: "", stderr: error.localizedDescription, exitCode: 1)
        }
        defer { try? FileManager.default.removeItem(at: tmp) }

        let proc = Process()
        proc.executableURL = URL(fileURLWithPath: exe)
        proc.arguments = [tmp.path]
        if let workingDirectory {
            proc.currentDirectoryURL = URL(fileURLWithPath: workingDirectory)
        }
        let out = Pipe()
        let err = Pipe()
        proc.standardOutput = out
        proc.standardError = err
        do {
            try proc.run()
            proc.waitUntilExit()
        } catch {
            return ExtensionRunResult(succeeded: false, stdout: "", stderr: error.localizedDescription, exitCode: 1)
        }
        let stdout = String(data: out.fileHandleForReading.readDataToEndOfFile(), encoding: .utf8) ?? ""
        let stderr = String(data: err.fileHandleForReading.readDataToEndOfFile(), encoding: .utf8) ?? ""
        return ExtensionRunResult(succeeded: proc.terminationStatus == 0, stdout: stdout, stderr: stderr, exitCode: proc.terminationStatus)
    }

    /// Parse SPSS-like BEGIN PROGRAM … END PROGRAM blocks from syntax text.
    public static func extractProgramBlocks(from text: String) -> [(runtime: ExtensionRuntime, code: String)] {
        var results: [(ExtensionRuntime, String)] = []
        let lines = text.components(separatedBy: .newlines)
        var current: ExtensionRuntime?
        var buffer: [String] = []
        for line in lines {
            let u = line.trimmingCharacters(in: .whitespacesAndNewlines).uppercased()
            if u.hasPrefix("BEGIN PROGRAM") {
                if u.contains("R") { current = .r }
                else { current = .python }
                buffer = []
                continue
            }
            if u.hasPrefix("END PROGRAM") {
                if let current {
                    results.append((current, buffer.joined(separator: "\n")))
                }
                current = nil
                buffer = []
                continue
            }
            if current != nil { buffer.append(line) }
        }
        return results
    }

    private static func probe(_ runtime: ExtensionRuntime, candidates: [String], missing: String) -> ExtensionCapability {
        for name in candidates {
            if let path = which(name) {
                return ExtensionCapability(runtime: runtime, available: true, interpreterPath: path,
                                           reason: "Using \(path) (isolated subprocess)")
            }
        }
        return ExtensionCapability(runtime: runtime, available: false, interpreterPath: nil, reason: missing)
    }

    private static func which(_ name: String) -> String? {
        let proc = Process()
        proc.executableURL = URL(fileURLWithPath: "/usr/bin/which")
        proc.arguments = [name]
        let out = Pipe()
        proc.standardOutput = out
        proc.standardError = Pipe()
        do {
            try proc.run()
            proc.waitUntilExit()
        } catch {
            return nil
        }
        guard proc.terminationStatus == 0 else { return nil }
        let data = out.fileHandleForReading.readDataToEndOfFile()
        let path = String(data: data, encoding: .utf8)?.trimmingCharacters(in: .whitespacesAndNewlines)
        return (path?.isEmpty == false) ? path : nil
    }
}

/// Compatibility alias for earlier Phase 8 stub name.
@available(*, deprecated, renamed: "ExtensionHost")
public typealias ExtensionsStub = ExtensionHost
