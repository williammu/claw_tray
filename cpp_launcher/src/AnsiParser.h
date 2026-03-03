#pragma once
#include "Common.h"
#include <regex>
#include <map>

namespace Launcher {

struct AnsiSegment {
    std::string text;
    std::string colorName;
};

class AnsiParser {
public:
    static AnsiParser& Instance() {
        static AnsiParser instance;
        return instance;
    }

    std::vector<AnsiSegment> Parse(const std::string& text) {
        std::vector<AnsiSegment> result;
        size_t pos = 0;
        
        static std::regex ansiRegex("\033\\[([0-9;]*)m");
        std::smatch match;
        std::string remaining = text;
        
        while (std::regex_search(remaining, match, ansiRegex)) {
            size_t matchPos = match.position();
            if (matchPos > 0) {
                result.push_back({remaining.substr(0, matchPos), currentColor_});
            }
            
            std::string code = match[1].str();
            ProcessCode(code);
            
            remaining = match.suffix();
        }
        
        if (!remaining.empty()) {
            result.push_back({remaining, currentColor_});
        }
        
        return result;
    }

    void Reset() {
        currentColor_ = "default";
    }

    COLORREF GetColor(const std::string& name) {
        static std::map<std::string, COLORREF> colors = {
            {"default", RGB(0, 255, 0)},
            {"black", RGB(0, 51, 0)},
            {"red", RGB(255, 51, 51)},
            {"green", RGB(0, 255, 0)},
            {"yellow", RGB(255, 204, 0)},
            {"blue", RGB(0, 204, 255)},
            {"magenta", RGB(255, 0, 255)},
            {"cyan", RGB(0, 255, 255)},
            {"white", RGB(204, 255, 204)},
            {"gray", RGB(0, 170, 0)},
            {"red_bright", RGB(255, 102, 102)},
            {"green_bright", RGB(57, 255, 20)},
            {"yellow_bright", RGB(255, 238, 102)},
            {"blue_bright", RGB(102, 221, 255)},
            {"magenta_bright", RGB(255, 102, 255)},
            {"cyan_bright", RGB(102, 255, 255)},
            {"white_bright", RGB(255, 255, 255)}
        };
        
        auto it = colors.find(name);
        return it != colors.end() ? it->second : colors["default"];
    }

private:
    std::string currentColor_ = "default";

    AnsiParser() = default;

    void ProcessCode(const std::string& code) {
        if (code == "0") {
            currentColor_ = "default";
            return;
        }
        
        static std::map<std::string, std::string> colorMap = {
            {"30", "black"}, {"31", "red"}, {"32", "green"}, {"33", "yellow"},
            {"34", "blue"}, {"35", "magenta"}, {"36", "cyan"}, {"37", "white"},
            {"90", "gray"}, {"91", "red_bright"}, {"92", "green_bright"},
            {"93", "yellow_bright"}, {"94", "blue_bright"},
            {"95", "magenta_bright"}, {"96", "cyan_bright"}, {"97", "white_bright"}
        };
        
        auto it = colorMap.find(code);
        if (it != colorMap.end()) {
            currentColor_ = it->second;
        }
    }
};

}
