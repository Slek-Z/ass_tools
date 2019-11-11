// ASS-Split - Split ASS subtitles
// Copyright (c) 2019 Slek

#define PROGRAM_NAME "ass_split"
#define PROGRAM_DESC "Split ASS subtitles."
#define PROGRAM_ARGS "input seconds out1 [out2]"
#define PROGRAM_VERS "1.0"
#define COPY_INFO "Copyright (c) 2019 Slek"

#define FLAGS_CASES                                                                                \
    FLAG_CASE(second_only, -1, "output only the second part on sigle output mode")

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "ass.hpp"
#include "flags.hpp"
#include "util/string.h"
#include "util/version.h"

inline void split(const ass::ASSFile& ass, const ass::time_t t, ass::ASSFile& first, ass::ASSFile& second) {
  first.clear();
  second.clear();

  first.BOM() = ass.BOM();
  second.BOM() = ass.BOM();

  first.ScriptComment() = ass.ScriptComment();
  second.ScriptComment() = ass.ScriptComment();

  bool has_events = false;
  for (const std::string& section : ass.Sections()) {
    if (section == ass::EVENTS) {
      has_events = true;

      const std::list<std::pair<std::string, std::string>>& lines = ass.Section(ass::EVENTS);

      std::list<std::pair<std::string, std::string>>::const_iterator it = lines.cbegin();
      if (it == lines.cend()) continue;

      const std::pair<std::string, std::string>& format_line = *it;
      first.add_line(ass::EVENTS, format_line.first, format_line.second);
      second.add_line(ass::EVENTS, format_line.first, format_line.second);
      ++it;

      if (format_line.first != "Format")
        throw ass::io_error("format line must appear first in events");

      std::size_t text_idx = std::numeric_limits<std::size_t>::max();
      if (!ass::get_field_index(format_line.second, "Text", &text_idx) ||
           (text_idx != (StringSplit(format_line.second, ass::FIELD_DELIMITER).size()-1)))
        throw ass::io_error("'Text' field must appear in last place");

      std::size_t start_idx = std::numeric_limits<std::size_t>::max();
      if (!ass::get_field_index(format_line.second, "Start", &start_idx))
        throw ass::io_error("'Start' field not found in format definition string");

      std::size_t end_idx = std::numeric_limits<std::size_t>::max();
      if (!ass::get_field_index(format_line.second, "End", &end_idx))
        throw ass::io_error("'End' field not found in format definition string");

      for (; it != lines.cend(); ++it) {
        const std::string& line_type = it->first;
        const std::string& line_data = it->second;

        ass::time_t start_ts = std::numeric_limits<ass::time_t>::max(),
          end_ts = std::numeric_limits<ass::time_t>::max();
        bool start_defined = false, end_defined = false;

        std::string::size_type start_begin, start_end;
        if (ass::get_field(line_data, start_idx, &start_begin, &start_end)) {
          if (start_begin < start_end) {
            start_ts = ass::parse_time(line_data.substr(start_begin, start_end - start_begin));
            start_defined = true;
          }
        } else throw ass::io_error("'Start' field cannot be retrieved");

        std::string::size_type end_begin, end_end;
        if (ass::get_field(line_data, end_idx, &end_begin, &end_end)) {
          if (end_begin < end_end) {
            end_ts = ass::parse_time(line_data.substr(end_begin, end_end - end_begin));
            end_defined = true;
          }
        } else throw ass::io_error("'End' field cannot be retrieved");

        // End ignored in command and sound events
        if ((line_type == ass::COMMAND_EVENT) || (line_type == ass::SOUND_EVENT)) end_defined = false;

        bool add1 = true, add2 = true;
        if (!start_defined) {
          end_defined = false;
        } else {
          if (start_ts >= t) {
            add1 = false;
            // Correct timestamps
            start_ts -= t;
            if (end_defined && (end_ts >= t)) end_ts -= t;
            else end_ts = 0;
          } else {
            add2 = false;
            if (end_defined && (end_ts > t))
              std::cerr << "[WARNING] Lossy split!" << std::endl;
          }
        }

        std::string start_str, end_str;
        if (start_defined)
          start_str = ass::format_time(start_ts);
        if (end_defined)
          end_str = ass::format_time(end_ts);

        std::stringstream event_data_stream;
        if (start_begin < end_begin) {
          event_data_stream << line_data.substr(0, start_begin)
                            << start_str
                            << line_data.substr(start_end, end_begin-start_end)
                            << end_str
                            << line_data.substr(end_end);
        } else if (end_begin < start_begin) {
          event_data_stream << line_data.substr(0, end_begin)
                            << end_str
                            << line_data.substr(end_end, start_begin-end_end)
                            << start_str
                            << line_data.substr(start_end);
        } else throw ass::io_error("unexpected error");

        if (add1)
          first.add_line(ass::EVENTS, line_type, event_data_stream.str());
        if (add2)
          second.add_line(ass::EVENTS, line_type, event_data_stream.str());
      }
    } else {
      first.insert(section, ass.Section(section));
      second.insert(section, ass.Section(section));
    }
  }

  if (!has_events)
    std::cerr << "[WARNING] Events section not found!" << std::endl;
}

int main(int argc, char* argv[]) {

  if (flags::HelpRequired(argc, argv)) {
    flags::ShowHelp();
    return 0; // SUCCESS
  }

  if (flags::VersionRequested(argc, argv)) {
    flags::ShowVersion();
    return 0; // SUCCESS
  }

  int result = flags::ParseFlags(&argc, &argv, true);
  if (result > 0) {
    std::cerr << "unrecognized option '" << argv[result] << "'" << std::endl;
    std::cerr << "Try '" << PROGRAM_NAME << " --help' for more information" << std::endl;
    return 1; // FAILURE
  }

  if (argc < 4 || argc > 5) {
    flags::ShowHelp();
    return 1; // FAILURE
  }

  if ((argc == 5) && FLAGS_second_only) {
    std::cerr << "--second_only option can only be useds in single output mode" << std::endl;
    return 1; //FAILURE
  }

  std::ifstream input(argv[1]);
  if (!input.is_open()) {
    std::cerr << "[ERROR] Can't open input file!" << std::endl;
    return 1; // FAILURE
  }

  double split_time = std::stod(argv[2]);
  if (!std::isnormal(split_time) || split_time < 0.0) {
    std::cerr << "[ERROR] Invalid split time!" << std::endl;
    return 1; // FAILURE
  }
  ass::time_t split_ts = ass::timestamp(split_time);

  std::ofstream* pout1 = nullptr;
  if ((argc == 5) || !FLAGS_second_only) {
    pout1 = new std::ofstream(argv[3]);
    if (!pout1->is_open()) {
      std::cerr << "[ERROR] Can't open" << ((argc == 5) ? " first " : " ") << "output file!" << std::endl;
      return 1; // FAILURE
    }
  }

  std::ofstream* pout2 = nullptr;
  if ((argc == 5) || FLAGS_second_only) {
    pout2 = new std::ofstream((argc == 5) ? argv[4] : argv[3]);
    if (!pout2->is_open()) {
      std::cerr << "[ERROR] Can't open" << ((argc == 5) ? " second " : " ") << "output file!" << std::endl;
      return 1; // FAILURE
    }
  }

  ass::ASSFile ass_input;
  ass_input.load(input);

  const std::string build = StringPrintf("; Script generated by ASSTools (%s)", GetBuildInfo().c_str());
  const std::string url = "; http://github.com/Slek-Z/ass_tools";
  ass_input.ScriptComment() = build + ass_input.LineBreak() + url;

  ass::ASSFile ass1, ass2;
  split(ass_input, split_ts, ass1, ass2);

  if (pout1) {
    *pout1 << ass1;
    delete pout1;
  }

  if (pout2) {
    *pout2 << ass2;
    delete pout2;
  }

  return 0; // SUCCESS
}
