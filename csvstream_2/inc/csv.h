#pragma once

#include <sing.h>

namespace csv_space {

class CsvStream final {
public:
    ~CsvStream();
    // intit from filename. returns false on failure.
    bool init(const char *filename, const char *delimiter = ",", bool strict = true);

    bool streamOperationFailed() const;

    // return the names of the fields of a csv row.
    void getHeader(std::vector<std::string> *header) const;

    // get the values from the next row. returns false on error (if the number of fields don't match the header)
    bool getRow(std::vector<std::string> *values);

    // if an error occurs, you can get a description from here
    std::string getErrorString() const;
};

}   // namespace
