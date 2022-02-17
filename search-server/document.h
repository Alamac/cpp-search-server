#pragma once

struct Document {
    Document() : id(0), relevance(0.0), rating(0) {};
    Document(const int& id, const double& relevance, const int& rating) : id(id), relevance(relevance), rating(rating) {};
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