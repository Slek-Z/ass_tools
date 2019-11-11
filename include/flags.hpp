// Header-only boolean flags handling (gflags style, simplified)
// Copyright (c) 2019 Slek

#ifndef FLAGS_HPP_
#define FLAGS_HPP_

// STL
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <ostream>
#include <string>
#include <unordered_map>

#include "util/version.h"

#define FLAG_VARIABLE(name)                                                                    \
    FLAGS_##name

#define DEFINE_FLAG(name)                                                                      \
    bool FLAG_VARIABLE(name) = false;

#define FLAG_CASE(name, short, txt)                                                            \
    DEFINE_FLAG(name)

#ifndef FLAGS_CASES
    #define FLAGS_CASES
#endif

FLAGS_CASES

#undef FLAG_CASE

#ifndef PROGRAM_NAME
    #define PROGRAM_NAME ""
#endif

#ifndef PROGRAM_DESC
    #define PROGRAM_DESC ""
#endif

#ifndef PROGRAM_VERS
    #define PROGRAM_VERS "1.0"
#endif

#ifndef COPY_INFO
    #define COPY_INFO ""
#endif

namespace flags {

bool HelpRequired(int argc, char* argv[]) {

    const std::string help_flag("--help");
    for (int i = 1; i < argc; ++i)
        if (help_flag.compare(argv[i]) == 0) return true;
    return false;
}

bool VersionRequested(int argc, char* argv[]) {

    const std::string version_flag("--version");
    for (int i = 1; i < argc; ++i)
        if (version_flag.compare(argv[i]) == 0) return true;
    return false;
}

void ShowHelp() {

    std::cout << "Usage: " << PROGRAM_NAME << " " << PROGRAM_ARGS;
    std::cout << std::endl;

    std::cout << PROGRAM_DESC << std::endl;

    //std::cout << std::endl;
    //std::cout << "With no FILE, or when FILE is -, read standard input." << std::endl;

    std::cout << std::endl;
    std::cout << "Options:" << std::endl;

    std::string::size_type n_max_name = 7; // min, for version
#define FLAG_CASE(name, short, txt)                                                            \
    n_max_name = std::max(n_max_name, std::string(#name).size());                              

    FLAGS_CASES

#undef FLAG_CASE

#define FLAG_CASE(name, short, txt)                                                            \
    if (short > 0)                                                                             \
        std::cout << "  -" << static_cast<char>(short) << ", --" << #name << "  ";             \
    else                                                                                       \
        std::cout << "      --" << #name << "  ";                                              \
    std::cout << std::string(n_max_name - std::string(#name).size(), ' ') << txt << std::endl; 

    FLAGS_CASES

#undef FLAG_CASE

    std::cout << std::endl;
    std::cout << "      --help  " << std::string(n_max_name - 4, ' ')
              << "display this help and exit" << std::endl;
    std::cout << "      --version  " << std::string(n_max_name - 7, ' ')
              << "output version information and exit" << std::endl;
    std::cout << std::endl;
}

void ShowVersion() {

    if (!std::string(PROGRAM_NAME).empty())
        std::cout << PROGRAM_NAME << " v" << PROGRAM_VERS;
    else
        std::cout << 'v' << PROGRAM_VERS;

    std::cout << " (" << GetBuildInfo() << ")";

    if (!std::string(COPY_INFO).empty())
        std::cout << std::endl << COPY_INFO;
    std::cout << std::endl;
}

int ParseFlags(int* argc, char*** argv, bool remove_flags = false) {

#define FLAG_CASE(name, short, txt)                                                            \
    , {#name, &FLAG_VARIABLE(name)}

    std::unordered_map<std::string, bool*> full_flags = { {"", nullptr} FLAGS_CASES };

#undef FLAG_CASE

#define FLAG_CASE(name, short, txt)                                                            \
    , {short, &FLAG_VARIABLE(name)}

    std::unordered_map<std::int8_t, bool*> short_flags = { {-1, nullptr} FLAGS_CASES };
    short_flags[-1] = nullptr;

#undef FLAG_CASE

    for (int i = 1; i < *argc; ++i) {
        std::string arg((*argv)[i]);

        if (arg.size() < 2) continue;
        if (arg[0] != '-') continue;

        bool is_flag = false;
        if (arg.size() == 2) {
            // Short flag
            is_flag = true;
            char flag = arg[1];
            if (short_flags.find(flag) != short_flags.end())
                *short_flags[flag] = true;
            else
                return i;
        } else if (arg[1] == '-') {
            // Full flag
            is_flag = true;
            std::string flag = arg.substr(2, arg.size() - 2);
            if (full_flags.find(flag) != full_flags.end())
                *full_flags[flag] = true;
            else
                return i;
        }

        if (is_flag && remove_flags) {
            // memcpy should be faster...
            for (int j = i + 1; j < *argc; ++j)
                (*argv)[j-1] = (*argv)[j];
            (*argc)--;
            i--;
        }
    }

    return 0;
}

} // namespace flags

#endif // FLAGS_HPP_