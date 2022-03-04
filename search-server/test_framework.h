#pragma once

#include <iostream>
#include <map>
#include <vector>
#include <set>

#include "document.h"

using namespace std::string_literals;

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))
#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

std::ostream& operator<<(std::ostream& os, const DocumentStatus& s){
    static std::map<DocumentStatus, std::string> status_to_string;
    if (status_to_string.size() == 0) {
        #define INSERT_ELEMENT(p) status_to_string[p] = #p
            INSERT_ELEMENT(DocumentStatus::ACTUAL);     
            INSERT_ELEMENT(DocumentStatus::IRRELEVANT);     
            INSERT_ELEMENT(DocumentStatus::BANNED);
            INSERT_ELEMENT(DocumentStatus::REMOVED);    
        #undef INSERT_ELEMENT
    }
    return os << status_to_string[s];
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& v) {
    if (v.empty()) {
        return os;
    }
    bool is_first = true;
    os << "[";
    for (const auto& el : v) {
        if(!is_first) {
            os << ", ";
        }
        os << el;
        is_first = false;
    }
    os << "]";
    return os;
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::set<T>& s) {
    if (s.empty()) {
        return os;
    }
    bool is_first = true;
    os << "{";
    for (const auto& el : s) {
        if(!is_first) {
            os << ", ";
        }
        os << el;
        is_first = false;
    }
    os << "}";
    return os;
}

template <typename T, typename V>
std::ostream& operator<<(std::ostream& os, const std::map<T, V>& m) {
    if (m.empty()) {
        return os;
    }
    bool is_first = true;
    os << "{";
    for (const auto& [key, value] : m) {
        if(!is_first) {
            os << ", ";
        }
        os << key << ": "s << value;
        is_first = false;
    }
    os << "}";
    return os;
}

template <typename T, typename V>
std::ostream& operator<<(std::ostream& os, const std::tuple<T, V> t) {
    os << "{"s << std::get<0>(t) << ", "s << std::get<1>(t) << "}"s;
    return os;
}

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
                     const std::string& func, unsigned line, const std::string& hint) {
    if (t != u) {
        std::cout << std::boolalpha;
        std::cout << file << "("s << line << "): "s << func << ": "s;
        std::cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        std::cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            std::cout << " Hint: "s << hint;
        }
        std::cout << std::endl;
        abort();
    }
}

void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
                const std::string& hint) {
    if (!value) {
        std::cout << file << "("s << line << "): "s << func << ": "s;
        std::cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            std::cout << " Hint: "s << hint;
        }
        std::cout << std::endl;
        abort();
    }
}