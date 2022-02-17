#include <string>
#include <iostream>

#include "document.h"

using namespace std::string_literals;

std::ostream& operator<<(std::ostream& os, Document doc) {
    os << "{ document_id = "s << doc.id << ", relevance = "s << doc.relevance << ", rating = "s << doc.rating << " }"s;
    return os;
}