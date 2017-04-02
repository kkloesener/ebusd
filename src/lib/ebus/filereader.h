/*
 * ebusd - daemon for communication with eBUS heating systems.
 * Copyright (C) 2014-2017 John Baier <ebusd@ebusd.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIB_EBUS_FILEREADER_H_
#define LIB_EBUS_FILEREADER_H_

#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <iomanip>
#include <mutex>
#include "lib/ebus/symbol.h"
#include "lib/ebus/result.h"

namespace ebusd {

/** @file lib/ebus/filereader.h
 * Helper class and constants for reading configuration files.
 *
 * The @a FileReader template class allows to read CSV compliant text files
 * while splitting each read line into fields.
 * It also supports special treatment of comment lines starting with a "#", as
 * well as so called "default values" indicated by the first field starting
 * with a "*" symbol.
 */

using std::string;
using std::map;
using std::ostream;
using std::istream;
using std::mutex;

/** the separator character used between fields. */
#define FIELD_SEPARATOR ','

/** the separator character used to quote text having the @a FIELD_SEPARATOR in it. */
#define TEXT_SEPARATOR '"'

/** the separator character as string used to quote text having the @a FIELD_SEPARATOR in it. */
#define TEXT_SEPARATOR_STR "\""

/** the separator character used between multiple values (in CSV only). */
#define VALUE_SEPARATOR ';'


/**
 * An abstract class that support reading definitions from a file.
 */
class FileReader {
 public:
  /**
   * Constructor.
   */
  FileReader() {}

  /**
   * Destructor.
   */
  virtual ~FileReader() {}

  /**
   * Read the definitions from a file.
   * @param filename the name of the file being read.
   * @param errorDescription a string in which to store the error description in case of error.
   * @param verbose whether to verbosely log problems.
   * @param defaults the default values by name (potentially overwritten by file name), or NULL to not use defaults.
   * @param hash optional pointer to a @a size_t value for storing the hash of the file, or NULL.
   * @param size optional pointer to a @a size_t value for storing the normalized size of the file, or NULL.
   * @param time optional pointer to a @a time_t value for storing the modification time of the file, or NULL.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t readFromFile(const string filename, string& errorDescription, bool verbose = false,
      map<string, string>* defaults = NULL, size_t* hash = NULL, size_t* size = NULL, time_t* time = NULL);

  /**
   * Read a single line definition from the stream.
   * @param stream the @a istream to read from.
   * @param errorDescription a string in which to store the error description in case of error.
   * @param filename the name of the file being read.
   * @param lineNo the last line number (incremented with each line read).
   * @param row the definition row to clear and update with the read data (for performance reasons only).
   * @param verbose whether to verbosely log problems.
   * @param hash optional pointer to a @a size_t value for updating with the hash of the line, or NULL.
   * @param size optional pointer to a @a size_t value for updating with the normalized length of the line, or NULL.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t readLineFromStream(istream& stream, string& errorDescription,
      const string filename, unsigned int& lineNo, vector<string>& row, bool verbose = false,
      size_t* hash = NULL, size_t* size = NULL);

  /**
   * Add a definition that was read from a file.
   * @param row the definition row.
   * @param errorDescription a string in which to store the error description in case of error.
   * @param filename the name of the file being read.
   * @param lineNo the current line number in the file being read.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t addFromFile(vector<string>& row, string& errorDescription,
      const string filename, unsigned int lineNo) = 0;

  /**
   * Left and right trim the string.
   * @param str the @a string to trim.
   */
  static void trim(string& str);

  /**
   * Convert all upper case characters in the string to lower case.
   * @param str the @a string to convert.
   */
  static void tolower(string& str);

  /**
   * Split the next line(s) from the @a istream into fields.
   * @param ifs the @a istream to read from.
   * @param row the @a vector to which to add the fields. This will be empty for completely empty and comment lines.
   * @param lineNo the current line number (incremented with each line read).
   * @param hash optional pointer to a @a size_t value for combining the hash of the line with, or NULL.
   * @param size optional pointer to a @a size_t value to add the trimmed line length to, or NULL.
   * @return true if there are more lines to read, false when there are no more lines left.
   */
  static bool splitFields(istream& ifs, vector<string>& row, unsigned int& lineNo,
      size_t* hash = NULL, size_t* size = NULL);

  /**
   * Format the specified hash as 8 hex digits to the output stream.
   * @param hash the hash code.
   * @param str the @a ostream to write to.
   */
  static void formatHash(size_t hash, ostream& str) {
    str << std::hex << std::setw(8) << std::setfill('0') << (hash & 0xffffffff) << std::dec << std::setw(0);
  }
};


/**
 * An abstract class derived from @a FileReader that additionally allows to using mapped name/value pairs with one
 * main map and many sub maps.
 */
class MappedFileReader : public FileReader {
 public:
  /**
   * Constructor.
   * @param supportsDefaults whether this instance supports rows with defaults (starting with a star).
   */
  explicit MappedFileReader(bool supportsDefaults) : FileReader(), m_supportsDefaults(supportsDefaults) {}

  /**
   * Destructor.
   */
  virtual ~MappedFileReader() {
    m_columnNames.clear();
    m_lastDefaults.clear();
    m_lastSubDefaults.clear();
  }

  // @copydoc
  result_t readFromFile(const string filename, string& errorDescription, bool verbose = false,
      map<string, string>* defaults = NULL, size_t* hash = NULL, size_t* size = NULL, time_t* time = NULL) override;

  /**
   * Extract default values from the file name.
   * @param filename the name of the file (without path)
   * @param defaults the default values by name to add to.
   * @param destAddress a pointer to a variable in which to store the numeric destination address, or NULL.
   * @param software a pointer to a in which to store the numeric software version, or NULL.
   * @param hardware a pointer to a in which to store the numeric hardware version, or NULL.
   * @return true if the minimum parts were extracted, false otherwise.
   */
  virtual bool extractDefaultsFromFilename(string filename, map<string, string>& defaults,
      symbol_t* destAddress = NULL, unsigned int* software = NULL, unsigned int* hardware = NULL) {
    return false;
  }

  // @copydoc
  result_t addFromFile(vector<string>& row, string& errorDescription,
      const string filename, unsigned int lineNo) override;

  /**
   * Get the field mapping from the given first line.
   * @param row the first line from which to extract the field mapping, or empty to use the default mapping.
   * @param errorDescription a string in which to store the error description in case of error.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t getFieldMap(vector<string>& row, string& errorDescription) = 0;

  /**
   * Add a default row that was read from a file.
   * @param row the default row by field name.
   * @param subRows the sub default rows, each by field name.
   * @param errorDescription a string in which to store the error description in case of error.
   * @param filename the name of the file being read.
   * @param lineNo the current line number in the file being read.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t addDefaultFromFile(map<string, string>& row, vector< map<string, string> >& subRows,
      string& errorDescription, const string filename, unsigned int lineNo) {
    errorDescription = "defaults not supported";
    return RESULT_ERR_INVALID_ARG;
  }

  /**
   * Add a definition that was read from a file.
   * @param row the main definition row by field name.
   * @param subRows the sub definition rows, each by field name.
   * @param errorDescription a string in which to store the error description in case of error.
   * @param filename the name of the file being read.
   * @param lineNo the current line number in the file being read.
   * @return @a RESULT_OK on success, or an error code.
   */
  virtual result_t addFromFile(map<string, string>& row, vector< map<string, string> >& subRows,
      string& errorDescription, const string filename, unsigned int lineNo) = 0;

  /**
   * @return a reference to all previously extracted default values by type and field name.
   */
  virtual map<string, map<string, string> >& getDefaults() {
    return m_lastDefaults;
  }

  /**
   * @return a reference to all previously extracted sub default values by type and field name.
   */
  virtual map<string, vector< map<string, string> > >& getSubDefaults() {
    return m_lastSubDefaults;
  }

  /**
   * Combine the row to a single string.
   * @param row the mapped row.
   * @return the combined string.
   */
  static string combineRow(const map<string, string>& row);

 private:
  /** whether this instance supports rows with defaults (starting with a star). */
  const bool m_supportsDefaults;

  /** a @a mutex for access to defaults. */
  mutex m_mutex;

  /** the name of each column. */
  vector<string> m_columnNames;

  /** all previously extracted default values by type and field name. */
  map<string, map<string, string> > m_lastDefaults;

  /** all previously extracted sub default values by type and field name. */
  map<string, vector< map<string, string> > > m_lastSubDefaults;
};

}  // namespace ebusd

#endif  // LIB_EBUS_FILEREADER_H_
