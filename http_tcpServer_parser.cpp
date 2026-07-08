#include "http_tcpServer_parser.h"
#include <string>
#include <algorithm>
#include <cctype>

namespace httpParser{



    std::string toLower(std::string message){
        for (auto &c : message) {
            c = std::tolower(static_cast<unsigned char>(c));
        }
        return message;
    }

    bool checkUpgradeRequest(std::string message){
        std::string lowerCase = toLower(message);
        bool hasUpgrade = lowerCase.find("upgrade: websocket") != std::string::npos;
        bool hasConnection = lowerCase.find("connection: upgrade") != std::string::npos;
        bool hasKey = lowerCase.find("sec-websocket-key:") != std::string::npos;
        bool hasVersion = lowerCase.find("sec-websocket-version: 13") != std::string::npos;
        return hasUpgrade && hasConnection && hasKey && hasVersion;

    }
}