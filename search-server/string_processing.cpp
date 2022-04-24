#include "string_processing.h"

#include <algorithm>

std::vector<std::string_view> SplitIntoWords(std::string_view str) {
    std::vector<std::string_view> result;
    str.remove_prefix(std::min(str.size(), str.find_first_not_of(" ")));
    const int64_t pos_end = str.npos;
    
    while (str.size() > 0) {
        if (str.front() == ' ') {
            str.remove_prefix(std::min(str.size(), str.find_first_not_of(" ")));
            if (str.size() == 0) {
                break;
            }
        }
        int64_t space = str.find(' ');
        if (space == pos_end) {
            result.push_back(str);
            break;
        }
        result.push_back(str.substr(0, space));
        str.remove_prefix(space);
    }

    return result;
}