#include <string>
#include <iostream>

#include "document.h"

using namespace std::string_literals;

Document::Document() : id(0), relevance(0.0), rating(0), status(DocumentStatus::REMOVED) {}
Document::Document(int id, double relevance, int rating) : id(id), relevance(relevance), rating(rating) {}
Document::Document(int id, double relevance, int rating, DocumentStatus status) : id(id), relevance(relevance), rating(rating), status(status) {}

std::ostream& operator<<(std::ostream& os, Document doc) {
    os << "{ document_id = "s << doc.id << ", relevance = "s << doc.relevance << ", rating = "s << doc.rating << " }"s;
    return os;
}