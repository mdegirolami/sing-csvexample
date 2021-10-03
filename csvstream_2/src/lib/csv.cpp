#include "csv.h"
#include "csvstream.h"

namespace csv_space {

CsvStream::CsvStream()
{
    impl_ = nullptr;
}

CsvStream::~CsvStream()
{
    if (impl_ != nullptr) {
        delete((csvstream*)impl_);
    }
}

bool CsvStream::init(const char *filename, const char *delimiter, bool strict)
{
    const std::string file = filename;
    csvstream *implementation = nullptr;
    try {
        implementation = new csvstream(file, delimiter == nullptr ? ',' : delimiter[0], strict);
    } catch (csvstream_exception exc) {
        error_ = exc.msg;
        return(false);
    }
    if (implementation == nullptr) {
        error_ = "Out of memory";
        return(false);
    }
    if (impl_ != nullptr) {
        delete((csvstream*)impl_);
    }
    impl_ = implementation;
    return(true);
}

bool CsvStream::streamOperationFailed() const
{
    return((bool)*((csvstream*)impl_));
}

// return the names of the fields of a csv row.
void CsvStream::getHeader(std::vector<std::string> *header) const
{
    *header = ((csvstream*)impl_)->getheader();
}

// get the values from the next row. returns false on error (if the number of fields don't match the header)
bool CsvStream::getRow(std::vector<std::string> *values)
{
    std::vector< std::pair<std::string, std::string> > row;
    try {
        *((csvstream*)impl_) >> row;
    } catch (csvstream_exception exc) {
        error_ = exc.msg;
        return(false);
    }
    int numcols = row.size();
    if (numcols < 1) {
        error_ = "Csv has 0 columns !";
        return(false);
    }
    values->clear();
    values->reserve(numcols);
    for (int ii = 0; ii < numcols; ++ii) {
        values->push_back(row[ii].second);
    }
    return(true);
}

// if an error occurs, you can get a description from here
std::string CsvStream::getErrorString() const
{
    return(error_);
}

}   // namespace