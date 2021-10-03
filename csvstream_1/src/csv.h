/* csvstream.h
 *
 * Andrew DeOrio <awdeorio@umich.edu>
 *
 * An easy-to-use CSV file parser for C++
 * https://github.com/awdeorio/csvstream
 */
#pragma once

#include <sing.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <exception>

namespace csvstream {

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

private:
    // Error description
    std::string error;

    // Filename.  Used for error messages.
    std::string filename;

    // Stream in CSV format
    std::ifstream is;

    // Delimiter between columns
    char delimiter;

    // Strictly enforce the number of values in each row.  Raise an exception if
    // a row contains too many values or too few compared to the header.  When
    // strict=false, ignore extra values and set missing values to empty string.
    bool strict;

    // Line no in file.  Used for error messages
    size_t line_no;

    // Store header column names
    std::vector<std::string> header;

    // Process header, the first line of the file
    bool read_header();

    // Disable copying because copying streams is bad!

    // On the other end this can cause errors when c++ code is compiled which are not caught by the sing
    // parser. In the future sing wont allow classes with finalize() - like this - to be copied.

    // CsvStream(const CsvStream &);
    // CsvStream & operator= (const CsvStream &);    
};

///////////////////////////////////////////////////////////////////////////////
// Implementation

// Read and tokenize one line from a stream
static bool read_csv_line(std::istream &is,
                          std::vector<std::string> &data,
                          char delimiter
                          ) {

  // Add entry for first token, start with empty string
  data.clear();
  data.push_back(std::string());

  // Process one character at a time
  char c = '\0';
  enum State {BEGIN, QUOTED, QUOTED_ESCAPED, UNQUOTED, UNQUOTED_ESCAPED, END};
  State state = BEGIN;
  while(is.get(c)) {
    switch (state) {
    case BEGIN:
      // We need this state transition to properly handle cases where nothing
      // is extracted.
      state = UNQUOTED;

      // Intended switch fallthrough.  Beginning with GCC7, this triggers an
      // error by default.  Disable the error for this specific line.
      #if __GNUG__ && __GNUC__ >= 7
      [[fallthrough]];
      #endif

    case UNQUOTED:
      if (c == '"') {
        // Change states when we see a double quote
        state = QUOTED;
      } else if (c == '\\') { //note this checks for a single backslash char
        state = UNQUOTED_ESCAPED;
        data.back() += c;
      } else if (c == delimiter) {
        // If you see a delimiter, then start a new field with an empty string
        data.push_back("");
      } else if (c == '\n' || c == '\r') {
        // If you see a line ending *and it's not within a quoted token*, stop
        // parsing the line.  Works for UNIX (\n) and OSX (\r) line endings.
        // Consumes the line ending character.
        state = END;
      } else {
        // Append character to current token
        data.back() += c;
      }
      break;

    case UNQUOTED_ESCAPED:
      // If a character is escaped, add it no matter what.
      data.back() += c;
      state = UNQUOTED;
      break;

    case QUOTED:
      if (c == '"') {
        // Change states when we see a double quote
        state = UNQUOTED;
      } else if (c == '\\') {
        state = QUOTED_ESCAPED;
        data.back() += c;
      } else {
        // Append character to current token
        data.back() += c;
      }
      break;

    case QUOTED_ESCAPED:
      // If a character is escaped, add it no matter what.
      data.back() += c;
      state = QUOTED;
      break;

    case END:
      if (c == '\n') {
        // Handle second character of a Windows line ending (\r\n).  Do
        // nothing, only consume the character.
      } else {
        // If this wasn't a Windows line ending, then put character back for
        // the next call to read_csv_line()
        is.unget();
      }

      // We're done with this line, so break out of both the switch and loop.
      goto multilevel_break; //This is a rare example where goto is OK
      break;

    default:
      assert(0);
      throw state;   // can't simply happen (all the enum values match a switch case)

    }//switch
  }//while

 multilevel_break:
  // Clear the failbit if we extracted anything.  This is to mimic the behavior
  // of getline(), which will set the eofbit, but *not* the failbit if a partial
  // line is read.
  if (state != BEGIN) is.clear();

  // Return status is the underlying stream's status
  return static_cast<bool>(is);
}

bool CsvStream::init(const char *filename, const char *delimiter, bool strict)
{
    this->filename = filename;
    this->delimiter = delimiter[0];
    this->strict = strict;
    line_no = 0;

    // Open file
    is.open(filename);
    if (!is.is_open()) {
        error = "Error opening file: " + this->filename;
        return(false);
    }

    // Process header
    read_header();
    return(true);
}

CsvStream::~CsvStream() {
  if (is.is_open()) is.close();
}

bool CsvStream::streamOperationFailed() const
{
    return static_cast<bool>(is);
}

void CsvStream::getHeader(std::vector<std::string> *header) const
{
    *header = this->header;
}

bool CsvStream::getRow(std::vector<std::string> *values)
{
  // Clear input row
  values->clear();
  values->reserve(header.size());

  // Read one line from stream, bail out if we're at the end
  if (!read_csv_line(is, *values, delimiter)) return (false);
  line_no += 1;

  // When strict mode is disabled, coerce the length of the data.  If data is
  // larger than header, discard extra values.  If data is smaller than header,
  // pad data with empty strings.
  if (!strict) {
    values->resize(header.size());
  }

  // Check length of data
  if (values->size() != header.size()) {
    error = "Number of items in row does not match header. " +
      filename + ":L" + std::to_string(line_no) + " " +
      "header.size() = " + std::to_string(header.size()) + " " +
      "row.size() = " + std::to_string(values->size()) + " "
      ;
    return(false);
  }

  return(true);
}

std::string CsvStream::getErrorString() const
{
    return(error);
}

bool CsvStream::read_header() {
  // read first line, which is the header
  if (!read_csv_line(is, header, delimiter)) {
      error = "error reading header";
      return(false);
  }
  return(true);
}

} // namespace