#ifndef MX_COMPILER_OPTION_PARSER_H
#define MX_COMPILER_OPTION_PARSER_H

#include "common.h"

std::tuple<int, std::string, std::string> ParseOptions(int argc, char *argv[]);

#endif