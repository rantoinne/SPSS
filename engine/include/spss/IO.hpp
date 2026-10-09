#pragma once

#include "DatasetStore.hpp"
#include <string>
#include <vector>

namespace spss {

struct ImportOptions {
    bool firstRowHeader{true};
    char delimiter{','};
    std::string sheetName; // Excel
    std::string encoding{"UTF-8"};
};

class DataIO {
public:
    static std::unique_ptr<DatasetStore> readCSV(const std::string& path, const ImportOptions& opt = {});
    static void writeCSV(const DatasetStore& ds, const std::string& path, const ImportOptions& opt = {});

    /// Minimal XLSX: reads first sheet sharedStrings + sheet1 as CSV-like via unzipped XML MVP,
    /// or tab-delimited fallback if file ends with .tsv.
    static std::unique_ptr<DatasetStore> readExcel(const std::string& path, const ImportOptions& opt = {});
    static void writeExcelCSV(const DatasetStore& ds, const std::string& path);

    /// SAV MVP: custom portable binary format compatible with our round-trip tests.
    /// Header magic "SPSSMAC1" + dictionary + columnar data. (Independent implementation,
    /// not IBM's proprietary binary layout — documented for interchange within this app.
    /// Also reads a simplified SPSS-like SAV subset when magic SYSMIS patterns match future work.)
    static std::unique_ptr<DatasetStore> readSAV(const std::string& path);
    static void writeSAV(const DatasetStore& ds, const std::string& path);
};

} // namespace spss
