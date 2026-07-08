#ifndef HTTP_PARSER
#define HTTP_PARSER

#include <string>

namespace httpParser{
    bool checkUpgradeRequest(std::string message);
}

#endif