#include "spss/DatasetStore.hpp"
#include "spss/Transform.hpp"
#include "spss/Procedure.hpp"
#include "spss/IO.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <fstream>
#include <filesystem>

using namespace spss;

static int failures = 0;

#define CHECK(cond) do { \
  if (!(cond)) { std::cerr << "FAIL: " << #cond << " at " << __FILE__ << ":" << __LINE__ << "\n"; ++failures; } \
} while (0)

#define CHECK_NEAR(a, b, eps) CHECK(std::fabs((a) - (b)) < (eps))

static DatasetStore sampleData() {
    DatasetStore ds("sample");
    Variable sex; sex.name = "sex"; sex.measure = Measure::Nominal;
    Variable age; age.name = "age";
    Variable score; score.name = "score";
    Variable group; group.name = "group"; group.measure = Measure::Nominal;
    ds.addVariable(sex);
    ds.addVariable(age);
    ds.addVariable(score);
    ds.addVariable(group);
    ds.setCaseCount(6);
    double sexes[] = {1,1,2,2,1,2};
    double ages[] = {20,22,30,28,25,35};
    double scores[] = {70,80,75,90,85,95};
    double groups[] = {1,1,1,2,2,2};
    for (size_t i = 0; i < 6; ++i) {
        ds.setNumeric(i, 0, sexes[i]);
        ds.setNumeric(i, 1, ages[i]);
        ds.setNumeric(i, 2, scores[i]);
        ds.setNumeric(i, 3, groups[i]);
    }
    return ds;
}

int main() {
    {
        auto ds = sampleData();
        CHECK(ds.caseCount() == 6);
        CHECK(ds.variableCount() == 4);
        ds.pushUndo();
        ds.setNumeric(0, 2, 99);
        CHECK_NEAR(ds.getNumeric(0, 2), 99, 1e-9);
        ds.undo();
        CHECK_NEAR(ds.getNumeric(0, 2), 70, 1e-9);
        ds.redo();
        CHECK_NEAR(ds.getNumeric(0, 2), 99, 1e-9);
    }

    {
        auto ds = sampleData();
        TransformationEngine::compute(ds, {"total", "age + score", "Age+Score"});
        CHECK(ds.hasVariable("total"));
        CHECK_NEAR(ds.getNumeric(0, ds.indexOf("total")), 90, 1e-9);
    }

    {
        auto ds = sampleData();
        RecodeSpec rec;
        rec.source = "sex";
        rec.target = "sex2";
        RecodeRule r; r.fromValue = 1; r.toValue = 10;
        rec.rules.push_back(r);
        RecodeRule e; e.fromElse = true; e.toCopy = true;
        rec.rules.push_back(e);
        TransformationEngine::recode(ds, rec);
        CHECK_NEAR(ds.getNumeric(0, ds.indexOf("sex2")), 10, 1e-9);
        CHECK_NEAR(ds.getNumeric(2, ds.indexOf("sex2")), 2, 1e-9);
    }

    {
        auto ds = sampleData();
        TransformationEngine::sortCases(ds, {{"score", false}});
        CHECK_NEAR(ds.getNumeric(0, ds.indexOf("score")), 95, 1e-9);
    }

    {
        auto ds = sampleData();
        auto out = ProcedureRunner::descriptives(ds, {{"age", "score"}});
        CHECK(!out.items().empty());
        auto html = out.toHTML();
        CHECK(html.find("Descriptive") != std::string::npos);
    }

    {
        auto ds = sampleData();
        auto out = ProcedureRunner::frequencies(ds, {{"sex"}, true, true});
        CHECK(out.items().size() >= 2);
    }

    {
        auto ds = sampleData();
        auto out = ProcedureRunner::crosstabs(ds, {{"sex"}, {"group"}, true, false, false, false});
        CHECK(!out.items().empty());
    }

    {
        auto ds = sampleData();
        auto out = ProcedureRunner::ttest(ds, {TTestOptions::Kind::OneSample, {"score"}, "", 1, 2, "", "", 80});
        CHECK(!out.items().empty());
    }

    {
        auto ds = sampleData();
        auto out = ProcedureRunner::regression(ds, {"score", {"age"}, true, false});
        CHECK(out.items().size() >= 2);
    }

    {
        auto ds = sampleData();
        namespace fs = std::filesystem;
        auto dir = fs::temp_directory_path() / "spss_engine_test";
        fs::create_directories(dir);
        auto csv = dir / "t.csv";
        auto sav = dir / "t.sav";
        DataIO::writeCSV(ds, csv.string());
        auto loaded = DataIO::readCSV(csv.string());
        CHECK(loaded->caseCount() == 6);
        DataIO::writeSAV(ds, sav.string());
        auto loadedSav = DataIO::readSAV(sav.string());
        CHECK(loadedSav->caseCount() == 6);
        CHECK(loadedSav->variableCount() == 4);
        CHECK_NEAR(loadedSav->getNumeric(1, 2), 80, 1e-9);
        CHECK(loadedSav->variableAt(0).name == "sex");
    }

    {
        auto ds = sampleData();
        auto out = ProcedureRunner::glmUnivariate(ds, {"score", {"group"}, {"age"}});
        CHECK(!out.items().empty());
        CHECK(out.toHTML().find("Between-Subjects") != std::string::npos ||
              out.toHTML().find("Parameter") != std::string::npos);
    }

    {
        auto ds = sampleData();
        auto out = ProcedureRunner::factorPCA(ds, {{"age", "score"}, 1});
        CHECK(out.toHTML().find("Component") != std::string::npos);
    }

    {
        auto ds = sampleData();
        auto out = ProcedureRunner::quickCluster(ds, {{"age", "score"}, 2, 25});
        CHECK(out.toHTML().find("Cluster") != std::string::npos);
    }

    {
        auto ds = sampleData();
        // binary-ish dependent from sex
        auto out = ProcedureRunner::logistic(ds, {"sex", {"age", "score"}, 30});
        CHECK(out.toHTML().find("Equation") != std::string::npos ||
              out.toHTML().find("Model") != std::string::npos);
    }

    {
        auto ds = sampleData();
        auto out = ProcedureRunner::ctables(ds, {"sex", "group", "score"});
        CHECK(out.toHTML().find("Custom Table") != std::string::npos);
    }

    {
        auto ds = sampleData();
        auto out = ProcedureRunner::fisherExact(ds, {"sex", "group"});
        CHECK(out.toHTML().find("Fisher") != std::string::npos);
    }

    {
        // Quoted commas + newlines inside fields (Google Forms–style survey CSV)
        namespace fs = std::filesystem;
        auto dir = fs::temp_directory_path() / "spss_engine_test";
        fs::create_directories(dir);
        auto csv = dir / "quoted_multiline.csv";
        {
            std::ofstream out(csv);
            out << "id,\"question\nwith newline\",\"income band\"\n";
            out << "1,\"hello, world\",\"₹50,000 - 99,999\"\n";
            out << "2,\"line1\nline2\",\"ok\"\n";
        }
        auto loaded = DataIO::readCSV(csv.string());
        CHECK(loaded->variableCount() == 3);
        CHECK(loaded->caseCount() == 2);
        CHECK(loaded->variableAt(1).type == VarType::String);
        CHECK(loaded->getString(0, 1).find("hello, world") != std::string::npos);
        CHECK(loaded->getString(1, 1).find("line1") != std::string::npos);
        CHECK(loaded->variableAt(1).label.find("question") != std::string::npos);
    }

    {
        // Real survey fixture if present in workspace
        namespace fs = std::filesystem;
        fs::path survey = fs::current_path() / "neha final poster.csv";
        if (!fs::exists(survey)) {
            survey = fs::current_path().parent_path() / "neha final poster.csv";
        }
        if (fs::exists(survey)) {
            auto loaded = DataIO::readCSV(survey.string());
            // Header has ~34 columns; naive line-split parsers inflate case count badly
            CHECK(loaded->variableCount() >= 30);
            CHECK(loaded->variableCount() <= 40);
            CHECK(loaded->caseCount() >= 100);
            CHECK(loaded->caseCount() <= 200);
            // Consent / age columns should not be shifted into the wrong type slots
            CHECK(!loaded->variableAt(0).name.empty());
        }
    }

    if (failures) {
        std::cerr << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "All engine tests passed\n";
    return 0;
}
