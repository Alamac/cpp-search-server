#pragma once

#include <map>

struct Document {
    Document();
    Document(int id, double relevance, int rating);
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

struct DocumentData {
    int rating;
    DocumentStatus status;
    std::map<std::string, double> word_count;
};

std::ostream& operator<<(std::ostream& os, Document doc);