// ASS-Time - Apply a linear transformation to ASS subtitles events
// Copyright (c) 2019 Slek

#define PROGRAM_NAME "ass_time"
#define PROGRAM_DESC "Apply a linear transformation to ASS subtitles events\n  (as t' = scale*t + offset)."
#define PROGRAM_ARGS "input offset [scale] output"
#define PROGRAM_VERS "1.0"
#define COPY_INFO "Copyright (c) 2019 Slek"

#define FLAGS_CASES                                                                                \

#include <cmath>
#include <cstdint>
#include <functional>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "ass.hpp"
#include "flags.hpp"
#include "util/string.h"
#include "util/version.h"

inline void transform(const ass::ASSFile& ass_in, const ass::time_signed_t offset, double scale, ass::ASSFile& ass_out) {
  ass_out.clear();

  ass_out.BOM() = ass_in.BOM();

  ass_out.ScriptComment() = ass_in.ScriptComment();

  bool has_events = false;
  for (const std::string& section : ass_in.Sections()) {
    if (section == ass::EVENTS) {
      has_events = true;

      const std::list<std::pair<std::string, std::string>>& lines = ass_in.Section(ass::EVENTS);

      std::list<std::pair<std::string, std::string>>::const_iterator it = lines.cbegin();
      if (it == lines.cend()) continue;

      const std::pair<std::string, std::string>& format_line = *it;
      if (format_line.first != "Format")
        throw ass::io_error("format line must appear first in events");

      ass_out.add_line(ass::EVENTS, format_line.first, format_line.second);
      ++it;

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

        ass::time_signed_t start_ts = std::numeric_limits<ass::time_signed_t>::max(),
          end_ts = std::numeric_limits<ass::time_signed_t>::max();
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

        if (!start_defined) {
          end_defined = false;
        } else {
          start_ts *= scale;
          start_ts += offset;
          if (end_defined) {
            end_ts *= scale;
            end_ts += offset;
          } else end_ts = 0;
        }
        if ((start_ts < 0) || (end_ts < 0))
          throw std::runtime_error("Transformation yields negative timestamps!");

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

        ass_out.add_line(ass::EVENTS, line_type, event_data_stream.str());
      }
    } else {
      ass_out.insert(section, ass_in.Section(section));
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

  std::ifstream input(argv[1]);
  if (!input.is_open()) {
    std::cerr << "[ERROR] Can't open input file!" << std::endl;
    return 1; // FAILURE
  }

  double offset_time = std::stod(argv[2]);
  if (!std::isfinite(offset_time)) {
    std::cerr << "[ERROR] Invalid offset time!" << std::endl;
    return 1; // FAILURE
  }
  ass::time_signed_t offset_ts = ass::timestamp_signed(offset_time);

  double scale = 1.0;
  std::ofstream output;
  if (argc == 4) {
    output.open(argv[3]);
  } else if (argc == 5) {
    scale = std::stod(argv[3]);
    if (!std::isnormal(scale) || scale < 0.0) {
      std::cerr << "[ERROR] Invalid scale!" << std::endl;
      return 1; // FAILURE
    }
    output.open(argv[4]);
  }

  if (!output.is_open()) {
    std::cerr << "[ERROR] Can't open output file!" << std::endl;
    return 1; // FAILURE
  }

  ass::ASSFile ass_input;
  ass_input.load(input);

  const std::string build = StringPrintf("; Script generated by ASSTools (%s)", GetBuildInfo().c_str());
  const std::string url = "; http://github.com/Slek-Z/ass_tools";
  ass_input.ScriptComment() = build + ass_input.LineBreak() + url;

  ass::ASSFile ass_output;
  transform(ass_input, offset_ts, scale, ass_output);

  output << ass_output;

  return 0; // SUCCESS
}
