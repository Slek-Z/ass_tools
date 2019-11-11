// ASS classes and functions
// Copyrught (c) 2019 Slek

#ifndef ASS_HPP_
#define ASS_HPP_

#include <cctype>
#include <cmath>
#include <exception>
#include <list>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <boost/filesystem.hpp>

#include "util/string.h"

namespace ass {

typedef std::uint32_t time_t;
typedef std::int32_t time_signed_t;

const std::string BOM = "\xef\xbb\xbf";

const std::string SCRIPT_INFO = "[Script Info]";
const std::string STYLES = "[V4+ Styles]";
const std::string FONTS = "[Fonts]";
const std::string GRAPHICS = "[Graphics]";
const std::string EVENTS = "[Events]";

const std::string COMMAND_EVENT = "Command";
const std::string COMMENT_EVENT = "Comment";
const std::string DIALOGUE_EVENT = "Dialogue";
const std::string MOVIE_EVENT = "Movie";
const std::string PICTURE_EVENT = "Picture";
const std::string SOUND_EVENT = "Sound";

const std::string FONT_LINE = "fontname";
const std::string FILE_LINE = "filename";

const std::string LINE_SEPARATOR = "\n";
const std::string FIELD_DELIMITER = ",";

const std::unordered_set<std::string> SECTIONS = {ass::SCRIPT_INFO, ass::STYLES, ass::FONTS, ass::GRAPHICS, ass::EVENTS};
const std::unordered_set<std::string> MULTILINE_FIELDS = {ass::FONT_LINE, ass::FILE_LINE};

class not_found : public std::exception {
public:

  not_found()
    : message_("ass::not_found") { }

  not_found(const char* message)
    : message_(message) { }

  inline const char * what () const noexcept {
    return message_.c_str();
  }

private:

  std::string message_;
};


class io_error : public std::exception {
public:

  io_error()
    : message_("ass::io_error") { }

  io_error(const char* message)
    : message_(message) { }

  inline const char * what () const noexcept {
    return message_.c_str();
  }

private:

  std::string message_;
};

inline ass::time_t timestamp(const double seconds) {
  return static_cast<ass::time_t>(seconds * 100.);
}

inline ass::time_signed_t timestamp_signed(const double seconds) {
  return static_cast<ass::time_signed_t>(seconds * 100.);
}

inline bool getline(std::ifstream& input, std::string& output, const std::string& delim = ass::LINE_SEPARATOR) {
  output.clear();

  if (!input.good()) return false;
  if (delim.empty()) return false;

  std::string buffer;
  buffer.reserve(delim.size());

  std::ifstream::char_type ch;
  std::string::size_type idx = 0;
  while (idx < delim.size()) {
    if ((ch = input.get()) == std::ifstream::traits_type::eof()) {
      output.append(buffer);
      return !output.empty();
    }
    if (delim.at(idx) == ch) {
      buffer.push_back(ch);
      idx++;
    } else {
      if (!buffer.empty()) {
        output.append(buffer);
        buffer.clear();
      }
      output.push_back(ch);
    }
  }

  return true;
}

inline bool defines_section(const std::string& input) {
  std::string input_ = input;
  StringTrim(&input_);
  return (StringStartsWith(input_, "[") && StringEndsWith(input_, "]"));
}

inline bool get_field_index(const std::string& format, const std::string& name, std::size_t* index, const std::string& delim = ass::FIELD_DELIMITER) {

  std::vector<std::string> v = StringSplit(format, delim);
  for (std::size_t i = 0; i < v.size(); ++i) {
    StringTrim(&v[i]);
    if (v[i] == name) {
      *index = i;
      return true;
    }
  }

  return false;
}

inline bool get_field(const std::string& str, std::size_t index, std::string::size_type* start, std::string::size_type* end, const std::string& delim = ass::FIELD_DELIMITER) {
  *start = 0;
  *end = std::string::npos;

  for (unsigned i = 0; i < index; ++i) {
    *end = str.find(delim, *start);
    if (*end == std::string::npos) {
      return false;
    }

    *start = *end + 1;
  }

  *end = str.find(delim, *start);
  return (*start < str.size());
}

inline std::vector<std::size_t> compute_permutation(const std::string& format1, const std::string& format2, const std::string& delim = ass::FIELD_DELIMITER) {

  std::vector<std::string> v2 = StringSplit(format2, delim);
  std::unordered_map<std::string, std::size_t> m2;
  for (std::size_t index = 0; index < v2.size(); ++index) {
    std::string& str = v2[index];
    StringTrim(&str);
    if (m2.find(str) != m2.end()) throw io_error("duplicated format field");
    m2[str] = index;
  }

  std::vector<std::string> v1 = StringSplit(format1, delim);
  std::vector<std::size_t> permutation(v1.size());
  std::unordered_set<std::string> s1;
  for (std::size_t index = 0; index < v1.size(); ++index) {
    std::string& str = v1[index];
    StringTrim(&str);
    if (s1.find(str) != s1.end()) throw io_error("duplicated format field");
    s1.insert(str);
    std::unordered_map<std::string, std::size_t>::const_iterator it = m2.find(str);
    if (it == m2.end()) throw io_error("incompatible line formats");
    permutation[index] = it->second;
  }

  return permutation;
}

inline bool apply_permutation(const std::vector<std::string>& v, const std::vector<std::size_t>& permutation, std::vector<std::string>& w) {
  w.clear();

  if (v.size() != permutation.size()) return false;
  w.reserve(permutation.size());

  for (std::size_t index = 0; index < v.size(); ++index) {
    std::size_t new_index = permutation[index];
    if (new_index >= v.size()) return false;
    w.push_back(v[new_index]);
  }

  return true;
}

inline ass::time_t parse_time(const std::string& time_str) {
  ass::time_t timestamp = 0;

  std::vector<std::string> v = StringSplit(time_str, ":");
  if (v.size() != 3) throw io_error("invalid timestamp format");

  unsigned long parsed_ul;
  double parsed_d;

  if (v[0].size() != 1) throw io_error("invalid timestamp format");
  parsed_ul = std::stoul(v[0]);
  timestamp += static_cast<ass::time_t>(parsed_ul * 360000ul);

  if (v[1].size() != 2) throw io_error("invalid timestamp format");
  parsed_ul = std::stoul(v[1]);
  if (parsed_ul >= 60ul) throw io_error("invalid timestamp format");
  timestamp += static_cast<ass::time_t>(parsed_ul * 6000ul);

  parsed_d = std::stod(v[2]);
  if (parsed_d >= 60.) throw io_error("invalid timestamp format");
  timestamp += static_cast<ass::time_t>(parsed_d * 100.);

  return timestamp;
}

inline std::string format_time(const ass::time_t timestamp) {
  if (timestamp >= 3600000ul) throw io_error("invalid timestamp value");

  ass::time_t remaining = timestamp;

  ass::time_t h = remaining / 360000ul;
  remaining -= h * 360000ul;

  ass::time_t min = remaining / 6000ul;
  remaining -= min * 6000ul;

  ass::time_t sec = remaining / 100ul;
  remaining -= sec * 100ul;

  return StringPrintf("%01u:%02u:%02u.%02u", h, min, sec, remaining);
}

class ASSFile {
public:

  /* Constructors */
  explicit ASSFile(bool has_bom = true)
    : has_bom_(has_bom), line_break_(ass::LINE_SEPARATOR)
  { }

  explicit ASSFile(const std::string& file)
    : has_bom_(true), line_break_(ass::LINE_SEPARATOR) {
    boost::filesystem::path filepath(file);
    if (!boost::filesystem::is_regular_file(filepath))
      throw not_found("file not found");

    std::ifstream input(filepath.string());
    load(input);
  }

  /* Getters-Setters */
  bool BOM() const { return has_bom_; }
  bool& BOM() { return has_bom_; }

  const std::string& LineBreak() const { return line_break_; }
  std::string& LineBreak() { return line_break_; }

  const std::string& ScriptComment() const { return script_comment_; }
  std::string& ScriptComment() { return script_comment_; }

  /* Getters */
  const std::list<std::pair<std::string, std::string>>& Section(const std::string& section) const { return sections_map_.at(section); }

  const std::unordered_set<std::string>& Sections() const { return sections_; }

  bool HasSection(const std::string& section) const {
    if ((sections_.find(section) != sections_.end()) && !sections_map_.at(section).empty())
      return true;
    return false;
  }

  /* Methods */
  void add_line(const std::string& section, const std::string& type, const std::string& data) {
    sections_.insert(section);
    sections_map_[section].push_back(std::make_pair(type, data));
  }

  void clear() {
    script_comment_.clear();
    sections_.clear();
    sections_map_.clear();
  }

  void insert(const std::string& section, const std::list<std::pair<std::string, std::string>>& data) {
    sections_.insert(section);
    for (const std::pair<std::string, std::string>& entry : data)
      add_line(section, entry.first, entry.second);
  }

  void load(std::ifstream& input) {
    clear();

    if (!input.is_open())
      throw io_error("can't open input file");

    std::string line;
    if (!getline(input, line, LINE_SEPARATOR))
      throw io_error("can't read input file");

    if (StringStartsWith(line, ass::BOM)) {
      has_bom_ = true;
      line = StringGetAfter(line, ass::BOM);
    } else {
      has_bom_ = false;
    }
    if (line.back() == '\r') line_break_ = "\r\n";
    StringTrim(&line);

    if (line != SCRIPT_INFO)
      throw io_error("input file isn't a valid V4 Script");

    bool skip_section = false;
    std::string current_type, current_data;
    std::string current_section = ass::SCRIPT_INFO;
    sections_.insert(current_section);
    while (getline(input, line, line_break_)) {
      if (line.front() == ';') continue;

      std::string trimmed_line = line;
      StringTrim(&trimmed_line);

      if (trimmed_line.empty()) continue; // No data? Ignore it then...

      if (defines_section(trimmed_line)) {
        // Terminate multi-line data, if needed
        if (!current_type.empty()) {
          add_line(current_section, current_type, current_data);

          current_type.clear();
          current_data.clear();
        }

        // Set flags
        skip_section = (ass::SECTIONS.find(trimmed_line) == ass::SECTIONS.end());
        if (!skip_section)
          current_section = trimmed_line;
      } else {
        // Data
        if (!skip_section && !current_type.empty()) {
          // Continue multi-line data (fonts and graphics)
          if (!std::islower(trimmed_line.front())) current_data += line_break_ + trimmed_line;

          // Multi-line data ends here?
          if (std::islower(trimmed_line.front()) || trimmed_line.size() < 80) {
            // Insert data
            add_line(current_section, current_type, current_data);

            // Clear temporals
            current_type.clear();
            current_data.clear();
            continue;
          }
        }

        if (!skip_section && current_type.empty()) {
          // New line
          std::string::size_type delim_pos = line.find(":");
          if (delim_pos == std::string::npos) {
            std::cout << line << std::endl;
            throw io_error("line type delimiter not found");
          }

          std::string type = line.substr(0, delim_pos);
          std::string data = line.substr(delim_pos+1);

          StringTrim(&type);
          if (ass::MULTILINE_FIELDS.find(type) != ass::MULTILINE_FIELDS.end()) {
            // Multi-line data
            current_type = type;
            current_data = data;
            continue;
          }

          // Single line data
          add_line(current_section, type, data);
        }
      }
    }

    if (!current_type.empty())
      add_line(current_section, current_type, current_data);
  }

  void remove_line(const std::string& section, std::list<std::pair<std::string, std::string>>::const_iterator it) {
    std::list<std::pair<std::string, std::string>>& data_list = sections_map_.at(section);

    data_list.erase(it);
    if (data_list.empty())
      remove_section(section);
  }

  void remove_section(const std::string& section) {
    sections_.erase(section);
    sections_map_.erase(section);
  }

private:

  bool has_bom_;

  std::string line_break_;
  std::string script_comment_;

  std::unordered_set<std::string> sections_;
  std::unordered_map<std::string, std::list<std::pair<std::string, std::string>>> sections_map_;
};

inline std::ostream& operator<<(std::ostream& lhs, const ASSFile& rhs) {

  if (rhs.BOM())
    lhs << ass::BOM;

  if (!rhs.HasSection(SCRIPT_INFO))
    throw io_error("missing 'ScriptInfo' section");

  const std::string& line_break = rhs.LineBreak();
  for (const std::string& section : {ass::SCRIPT_INFO, ass::STYLES, ass::FONTS, ass::GRAPHICS, ass::EVENTS}) {
    if (!rhs.HasSection(section)) continue;
    if (section != SCRIPT_INFO) lhs << line_break;
    lhs << section;

    if ((section == ass::SCRIPT_INFO) && !rhs.ScriptComment().empty())
      lhs << line_break << rhs.ScriptComment();

    for (const std::pair<std::string, std::string>& entry : rhs.Section(section))
      lhs << line_break << entry.first << ":" << entry.second;
    lhs << line_break;
  }

  return lhs;
}

} // namespace ass

#endif // ASS_HPP_