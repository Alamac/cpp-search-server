#pragma once

#include <vector>
#include <string>
#include <string_view>
#include <set>

std::vector<std::string_view> SplitIntoWords(std::string_view str);

using TransparentStringSet = std::set<std::string, std::less<>>;

template <typename C>
TransparentStringSet MakeUniqueNonEmptyStrings(const C& strings) {
    TransparentStringSet non_empty;
    for (std::string_view s : strings) {
        if (!s.empty()) {
            non_empty.emplace(s);
        }
    }
    return non_empty;
}