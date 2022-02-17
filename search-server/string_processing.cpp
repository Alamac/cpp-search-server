#include "string_processing.h"

std::vector<std::string> SplitIntoWords(const std::string& text) {
    std::vector<std::string> words;
    std::string word;
    for (const char& c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

bool StringHasSpecialSymbols(const std::string& s) {
    for (const auto& c : s) {
            if (int(c) <= 31 && int(c) >= 0) {
                return true;
            } 
        }
    return false;
}