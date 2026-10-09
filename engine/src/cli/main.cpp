#include "spss/DatasetStore.hpp"
#include "spss/IO.hpp"
#include "spss/Procedure.hpp"
#include "spss/Transform.hpp"
#include <iostream>
#include <string>

using namespace spss;

static void usage() {
    std::cerr << "StatDesk CLI\n"
              << "  spss_cli descriptives <csv> <var>...\n"
              << "  spss_cli frequencies <csv> <var>...\n"
              << "  spss_cli compute <csv> <target> <expr> <out.csv>\n";
}

int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 1; }
    std::string cmd = argv[1];
    try {
        if (cmd == "descriptives" || cmd == "frequencies") {
            if (argc < 4) { usage(); return 1; }
            auto ds = DataIO::readCSV(argv[2]);
            std::vector<std::string> vars;
            for (int i = 3; i < argc; ++i) vars.push_back(argv[i]);
            OutputDocument out;
            if (cmd == "descriptives") out = ProcedureRunner::descriptives(*ds, {vars});
            else out = ProcedureRunner::frequencies(*ds, {vars, true, false});
            std::cout << out.toHTML();
            return 0;
        }
        if (cmd == "compute") {
            if (argc < 6) { usage(); return 1; }
            auto ds = DataIO::readCSV(argv[2]);
            TransformationEngine::compute(*ds, {argv[3], argv[4], ""});
            DataIO::writeCSV(*ds, argv[5]);
            return 0;
        }
        usage();
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
