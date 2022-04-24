#pragma once

#include <iostream>

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};


struct Document {
    Document();
    Document(int id, double relevance, int rating);
    Document(int id, double relevance, int rating, DocumentStatus status);
    int id;
    double relevance;
    int rating;
    DocumentStatus status;
};

std::ostream& operator<<(std::ostream& os, Document doc);