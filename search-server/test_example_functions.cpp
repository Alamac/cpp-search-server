
#include <iostream>
#include <map>
#include <vector>
#include <set>

#include "test_example_functions.h"
#include "search_server.h"
#include "paginator.h"
#include "remove_duplicates.h"

using namespace std;

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

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = {1, 2, 3};

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"sv);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT(doc0.id == doc_id);
    }

    {
        SearchServer server("in the"sv);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"sv).empty());
    }
}

void TestMinusWords() {
    const int doc_id = 42;
    const std::string_view content = "cat in the city"sv;
    const std::vector<int> ratings = {1, 2, 3};

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("cat -city"sv).empty());
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.FindTopDocuments("cat city"sv).size(), 1);
    }

}

void TestDocumentMatching() {
    const int doc_id = 42;
    const std::string_view content = "cat in the city"sv;
    const std::vector<int> ratings = {1, 2, 3};

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.MatchDocument("cat city"sv, doc_id), std::make_tuple(std::vector<std::string_view> {"cat", "city"}, DocumentStatus::ACTUAL));
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.MatchDocument("cat -city"sv, doc_id), std::make_tuple(std::vector<std::string_view> {}, DocumentStatus::ACTUAL));
    }
}

void TestSortByRelevance() {
    const std::vector<int> ratings = {1, 2, 3};
    SearchServer server;
    server.AddDocument(10, "a b"sv, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(15, "a b c"sv, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(20, "a"sv, DocumentStatus::ACTUAL, ratings);
    std::vector<Document> docs = server.FindTopDocuments("a b c"sv);
    ASSERT_EQUAL(docs.size(), 3);
    ASSERT_EQUAL(docs[0].id, 15);
    ASSERT_EQUAL(docs[1].id, 10);
    ASSERT_EQUAL(docs[2].id, 20);
}

void TestCalcDocumentRating() {
    const std::vector<int> ratings_positive = {5, 15, 35, 45, 50};
    const std::vector<int> ratings_negative = {-5, -15, -35, -45, -50};
    const std::vector<int> ratings_mixed = {-5, 15, -35, 45, -50};
    {
        SearchServer server;
        server.AddDocument(20, "a", DocumentStatus::ACTUAL, ratings_positive);
        std::vector<Document> docs = server.FindTopDocuments("a"sv);
        ASSERT_EQUAL(docs.size(), 1);
        ASSERT_EQUAL(docs[0].rating, 30.0);
    }

    {
        SearchServer server;
        server.AddDocument(20, "a", DocumentStatus::ACTUAL, ratings_negative);
        std::vector<Document> docs = server.FindTopDocuments("a"sv);
        ASSERT_EQUAL(docs.size(), 1);
        ASSERT_EQUAL(docs[0].rating, -30.0);
    }

    {
        SearchServer server;
        server.AddDocument(20, "a", DocumentStatus::ACTUAL, ratings_mixed);
        std::vector<Document> docs = server.FindTopDocuments("a"sv);
        ASSERT_EQUAL(docs.size(), 1);
        ASSERT_EQUAL(docs[0].rating, -6.0);
    }
}

void TestUserFilterPredicate() {
    SearchServer server;
    server.AddDocument(1, "a b c d e"sv, DocumentStatus::ACTUAL, {1, 2, 3, 4, 5});
    server.AddDocument(2, "a b c"sv, DocumentStatus::BANNED, {10, 20, 30, 40, 50});
    server.AddDocument(5, "a b"sv, DocumentStatus::IRRELEVANT, {100, 200, 300, 400, 500});

    {
        std::vector<Document> docs = server.FindTopDocuments("a b c d e"sv, [](int document_id, DocumentStatus status, double rating){return document_id > 3;});
        ASSERT_EQUAL(docs.size(), 1);
        ASSERT_EQUAL(docs[0].id, 5);
    }

    {
        std::vector<Document> docs = server.FindTopDocuments("a b c d e"sv, [](int document_id, DocumentStatus status, double rating){return rating > 3;});
        ASSERT_EQUAL(docs.size(), 2);
        ASSERT_EQUAL(docs[0].id, 2);
        ASSERT_EQUAL(docs[1].id, 5);
    }

    {
        std::vector<Document> docs = server.FindTopDocuments("a b c d e"sv, [](int document_id, DocumentStatus status, double rating){return status == DocumentStatus::REMOVED;});
        ASSERT_EQUAL(docs.size(), 0);
    }
}

void TestDocumentStatusFilter() {
    SearchServer server;
    server.AddDocument(1, "a b c d e"sv, DocumentStatus::ACTUAL, {1, 2, 3, 4, 5});
    server.AddDocument(3, "a b c d e f"sv, DocumentStatus::ACTUAL, {1, 2, 3, 4, 5});
    server.AddDocument(2, "a b c"sv, DocumentStatus::BANNED, {10, 20, 30, 40, 50});
    server.AddDocument(5, "a b"sv, DocumentStatus::IRRELEVANT, {100, 200, 300, 400, 500});

    {
        std::vector<Document> docs = server.FindTopDocuments("a b c d e"sv, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(docs.size(), 2);
    }

    {
        std::vector<Document> docs = server.FindTopDocuments("a b c d e"sv, DocumentStatus::BANNED);
        ASSERT_EQUAL(docs.size(), 1);
        ASSERT_EQUAL(docs[0].id, 2);
    }

    {
        std::vector<Document> docs = server.FindTopDocuments("a b c d e"sv, DocumentStatus::REMOVED);
        ASSERT_EQUAL(docs.size(), 0);
    }
}

void TestCalcRelevance() {
    SearchServer server;
    server.AddDocument(1, "a b c d e"sv, DocumentStatus::ACTUAL, {1, 2, 3, 4, 5});
    server.AddDocument(3, "a b c d e f"sv, DocumentStatus::ACTUAL, {1, 2, 3, 4, 5});
    server.AddDocument(2, "a b c"sv, DocumentStatus::ACTUAL, {10, 20, 30, 40, 50});
    server.AddDocument(5, "a a b"sv, DocumentStatus::ACTUAL, {100, 200, 300, 400, 500});
    std::vector<Document> docs = server.FindTopDocuments("a b c d"sv);

    ASSERT_EQUAL(docs.size(), 4);
    ASSERT(std::abs(docs[0].relevance - 0.196166) <= RELEVANCE_THRESHOLD);
    ASSERT(std::abs(docs[1].relevance - 0.163471) <= RELEVANCE_THRESHOLD);
    ASSERT(std::abs(docs[2].relevance - 0.095894) <= RELEVANCE_THRESHOLD);
    ASSERT_EQUAL(docs[3].relevance, 0);
}

void TestPaginator() {
    SearchServer search_server("and with"sv);

    search_server.AddDocument(1, "funny pet and nasty rat"sv, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "funny pet with curly hair"sv, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat nasty hair"sv, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog cat Vladislav"sv, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog hamster Borya"sv, DocumentStatus::ACTUAL, {1, 1, 1});

    const auto search_results = search_server.FindTopDocuments("curly dog"sv);
    int page_size = 2;
    const auto pages = Paginate(search_results, page_size);
    ASSERT_EQUAL(pages.size(), 2);   
}

void TestGetWordFrequencies() {
    SearchServer search_server("and with"sv);

    search_server.AddDocument(5, "big dog hamster Borya big wife husband heck go out"sv, DocumentStatus::ACTUAL, {1, 1, 1});

    const std::map<std::string_view, double> test_case = {
        {"big"sv, 0.2},
        {"dog"sv, 0.1},
        {"hamster"sv, 0.1},
        {"Borya"sv, 0.1},
        {"big"sv, 0.1},
        {"wife"sv, 0.1},
        {"husband"sv, 0.1},
        {"heck"sv, 0.1},
        {"go"sv, 0.1},
        {"out"sv, 0.1}
    };

    ASSERT_EQUAL(search_server.GetWordFrequencies(5), test_case);
}

void TestRemoveDocument() {
    SearchServer search_server("and with"sv);

    search_server.AddDocument(1, "funny pet and nasty rat"sv, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "funny pet with curly hair"sv, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat nasty hair"sv, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog cat Vladislav"sv, DocumentStatus::ACTUAL, {1, 3, 2});

    auto word_freqs_before = search_server.GetWordToFreqs();
    search_server.AddDocument(5, "big dog hamster Borya"sv, DocumentStatus::ACTUAL, {1, 1, 1});
    search_server.RemoveDocument(5);
    ASSERT_EQUAL(word_freqs_before, search_server.GetWordToFreqs());
    auto word_freqs = search_server.GetWordFrequencies(5);
    ASSERT(word_freqs.empty());
}

void TestRemoveDuplicate() {
    SearchServer search_server("and with"sv);

    search_server.AddDocument(1, "funny pet and nasty rat"sv, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "funny pet with curly hair"sv, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(30, "big cat nasty hair"sv, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(40, "big dog cat Vladislav"sv, DocumentStatus::ACTUAL, {1, 3, 2});

    RemoveDuplicates(search_server);
    ASSERT_EQUAL(search_server.GetDocumentCount(), 4);

    search_server.AddDocument(5, "big dog cat Vladislav"sv, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(6, "big cat nasty hair"sv, DocumentStatus::ACTUAL, {1, 2, 8});
    RemoveDuplicates(search_server);
    ASSERT_EQUAL(search_server.GetDocumentCount(), 4);
}

void TestSearchServer() {
    std::cout << "Module tests started!"s << std::endl;
    TestExcludeStopWordsFromAddedDocumentContent();
    TestMinusWords();
    TestDocumentMatching();
    TestSortByRelevance();
    TestCalcDocumentRating();
    TestUserFilterPredicate();
    TestDocumentStatusFilter();
    TestCalcRelevance();
    TestPaginator();
    TestGetWordFrequencies();
    TestRemoveDocument();
    TestRemoveDuplicate();
    std::cout << "Module tests completed successfully!"s << std::endl;
}