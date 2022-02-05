#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <numeric>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double RELEVANCE_THRESHOLD = 1e-6;

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

///////////////                              ///////////////
///////////////         TEST FRAMEWORK       ///////////////
///////////////                              ///////////////

ostream& operator<<(ostream& os, const DocumentStatus& s){
    static map<DocumentStatus, string> status_to_string;
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

template <typename T, typename V>
ostream& operator<<(ostream& os, const tuple<T, V> t) {
    os << "{"s << get<0>(t) << ", "s << get<1>(t) << "}"s;
    return os;
}

template <typename T>
ostream& operator<<(ostream& os, const vector<T>& v) {
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
ostream& operator<<(ostream& os, const set<T>& s) {
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
ostream& operator<<(ostream& os, const map<T, V>& m) {
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

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

///////////////                              ///////////////
///////////////     END OF TEST FRAMEWORK    ///////////////
///////////////                              ///////////////

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
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

bool StringHasSpecialSymbols(const string& s) {
    for (const auto& c : s) {
            if (int(c) <= 31 && int(c) >= 0) {
                return true;
            } 
        }
    return false;
}

class SearchServer {
public:

    SearchServer() {
    }

    SearchServer(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
        if (StringHasSpecialSymbols(word)) {
            throw invalid_argument("There is a special symbol in stopword: "s + word);
        }
        stop_words_.insert(word);
        }
    }

    template<typename C, typename T = typename C::value_type>
    SearchServer(const C& container) {
        for (const string& word : container) {
            if (StringHasSpecialSymbols(word)) {
                throw invalid_argument("There is a special symbol in stopword: "s + word);
            }
            if (word != ""s) {
                stop_words_.insert(word);
            }
        }
    }

    int GetDocumentCount() const {
        return documents_.size();
    }
    
    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        if (document_id < 0) {
            throw invalid_argument("Document id("s + to_string(document_id) + ") is less then 0"s);
        }
        if (documents_.find(document_id) != documents_.end()) {
            throw invalid_argument("There is already a document in document list with id: "s + to_string(document_id));
        }
        if (StringHasSpecialSymbols(document)) {
            throw invalid_argument("There is a special symbol in document: "s + document);
        }

        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, 
            DocumentData{
                ComputeAverageRating(ratings), 
                status
            });
    }

    //to use with filter lambda
    template<typename Filter>
    vector<Document> FindTopDocuments(const string& raw_query, Filter FilterLamdaFunc) const {            
        const Query query = ParseQuery(raw_query);
        vector<Document> matched_documents = FindAllDocuments(query);
        
        SortDocuments(matched_documents);

        vector<Document> filtered_documents;

        for (const auto& doc : matched_documents) {
            if (FilterLamdaFunc(doc.id, documents_.at(doc.id).status, documents_.at(doc.id).rating)) {
                filtered_documents.push_back(doc);
            }
        }

        ApplyMaxResultDocumentCount(filtered_documents);
        return filtered_documents;
    }

    //to use with DocumentStatus
    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {   
        return FindTopDocuments(raw_query, [status](int doc_id, DocumentStatus doc_status, int doc_rating) 
            {
                return doc_status == status;
            }
        );
    }

    //to use without second arg at all (using DocumentStatus::ACTUAL)
    vector<Document> FindTopDocuments(const string& raw_query) const {            
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }
    
    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

    int GetDocumentId(int index) const {
        if (index >= documents_.size()) {
            throw out_of_range("Document list size is less then "s + to_string(index));
        }
        else {
            auto it = documents_.begin();
            advance(it, index);
            return it->first;
        }
    }
    
private:
    //structs
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    //vars
    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    
    //methods
    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }
    
    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }
    
    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }
    
    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
             if (word[0] == '-' && word[1] == '-') {
                throw invalid_argument("There is a word with double minus(--) in the search query");
            }
            if (word[0] == '-' && word.size() == 1) {
                throw invalid_argument("There is a single minus( - ) in the search query");
            }
            if (StringHasSpecialSymbols(word)) {
                throw invalid_argument("There is a special symbol in the search query");
            }
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }
    
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    vector<Document> FindAllDocuments(const Query& query) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
        
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({
                document_id,
                relevance,
                documents_.at(document_id).rating
            });
        }
        return matched_documents;
    }

    //static methods
    static void SortDocuments(vector<Document>& documents) {
        sort(documents.begin(), documents.end(),
             [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < RELEVANCE_THRESHOLD) {
                    return lhs.rating > rhs.rating;
                } else {
                    return lhs.relevance > rhs.relevance;
                }
             });
    }

    static void ApplyMaxResultDocumentCount(vector<Document>& docs) {
        if (docs.size() > MAX_RESULT_DOCUMENT_COUNT) {
            docs.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
        return rating_sum / static_cast<int>(ratings.size());
    }

};


// -------- Начало модульных тестов поисковой системы ----------

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT(doc0.id == doc_id);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}

void TestMinusWords() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("cat -city").empty());
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.FindTopDocuments("cat city").size(), 1);
    }

}

void TestDocumentMatching() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.MatchDocument("cat city", doc_id), tuple(vector<string> {"cat", "city"}, DocumentStatus::ACTUAL));
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.MatchDocument("cat -city", doc_id), tuple(vector<string> {}, DocumentStatus::ACTUAL));
    }
}

void TestSortByRelevance() {
    const vector<int> ratings = {1, 2, 3};
    SearchServer server;
    server.AddDocument(10, "a b", DocumentStatus::ACTUAL, ratings);
    server.AddDocument(15, "a b c", DocumentStatus::ACTUAL, ratings);
    server.AddDocument(20, "a", DocumentStatus::ACTUAL, ratings);
    vector<Document> docs = server.FindTopDocuments("a b c");
    ASSERT_EQUAL(docs.size(), 3);
    ASSERT_EQUAL(docs[0].id, 15);
    ASSERT_EQUAL(docs[1].id, 10);
    ASSERT_EQUAL(docs[2].id, 20);
}

void TestCalcDocumentRating() {
    const vector<int> ratings_positive = {5, 15, 35, 45, 50};
    const vector<int> ratings_negative = {-5, -15, -35, -45, -50};
    const vector<int> ratings_mixed = {-5, 15, -35, 45, -50};
        {
            SearchServer server;
            server.AddDocument(20, "a", DocumentStatus::ACTUAL, ratings_positive);
            vector<Document> docs = server.FindTopDocuments("a");
            ASSERT_EQUAL(docs.size(), 1);
            ASSERT_EQUAL(docs[0].rating, 30.0);
        }

        {
            SearchServer server;
            server.AddDocument(20, "a", DocumentStatus::ACTUAL, ratings_negative);
            vector<Document> docs = server.FindTopDocuments("a");
            ASSERT_EQUAL(docs.size(), 1);
            ASSERT_EQUAL(docs[0].rating, -30.0);
        }

        {
            SearchServer server;
            server.AddDocument(20, "a", DocumentStatus::ACTUAL, ratings_mixed);
            vector<Document> docs = server.FindTopDocuments("a");
            ASSERT_EQUAL(docs.size(), 1);
            ASSERT_EQUAL(docs[0].rating, -6.0);
        }
}

void TestUserFilterPredicate() {
    SearchServer server;
    server.AddDocument(1, "a b c d e", DocumentStatus::ACTUAL, {1, 2, 3, 4, 5});
    server.AddDocument(2, "a b c", DocumentStatus::BANNED, {10, 20, 30, 40, 50});
    server.AddDocument(5, "a b", DocumentStatus::IRRELEVANT, {100, 200, 300, 400, 500});

    {
        vector<Document> docs = server.FindTopDocuments("a b c d e", [](int document_id, DocumentStatus status, double rating){return document_id > 3;});
        ASSERT_EQUAL(docs.size(), 1);
        ASSERT_EQUAL(docs[0].id, 5);
    }

    {
        vector<Document> docs = server.FindTopDocuments("a b c d e", [](int document_id, DocumentStatus status, double rating){return rating > 3;});
        ASSERT_EQUAL(docs.size(), 2);
        ASSERT_EQUAL(docs[0].id, 2);
        ASSERT_EQUAL(docs[1].id, 5);
    }

    {
        vector<Document> docs = server.FindTopDocuments("a b c d e", [](int document_id, DocumentStatus status, double rating){return status == DocumentStatus::REMOVED;});
        ASSERT_EQUAL(docs.size(), 0);
    }
}

void TestDocumentStatusFilter() {
    SearchServer server;
    server.AddDocument(1, "a b c d e", DocumentStatus::ACTUAL, {1, 2, 3, 4, 5});
    server.AddDocument(3, "a b c d e f", DocumentStatus::ACTUAL, {1, 2, 3, 4, 5});
    server.AddDocument(2, "a b c", DocumentStatus::BANNED, {10, 20, 30, 40, 50});
    server.AddDocument(5, "a b", DocumentStatus::IRRELEVANT, {100, 200, 300, 400, 500});

    {
        vector<Document> docs = server.FindTopDocuments("a b c d e", DocumentStatus::ACTUAL);
        ASSERT_EQUAL(docs.size(), 2);
    }

    {
        vector<Document> docs = server.FindTopDocuments("a b c d e", DocumentStatus::BANNED);
        ASSERT_EQUAL(docs.size(), 1);
        ASSERT_EQUAL(docs[0].id, 2);
    }

    {
        vector<Document> docs = server.FindTopDocuments("a b c d e", DocumentStatus::REMOVED);
        ASSERT_EQUAL(docs.size(), 0);
    }
}

void TestCalcRelevance() {
    SearchServer server;
    server.AddDocument(1, "a b c d e", DocumentStatus::ACTUAL, {1, 2, 3, 4, 5});
    server.AddDocument(3, "a b c d e f", DocumentStatus::ACTUAL, {1, 2, 3, 4, 5});
    server.AddDocument(2, "a b c", DocumentStatus::ACTUAL, {10, 20, 30, 40, 50});
    server.AddDocument(5, "a a b", DocumentStatus::ACTUAL, {100, 200, 300, 400, 500});
    vector<Document> docs = server.FindTopDocuments("a b c d");

    ASSERT_EQUAL(docs.size(), 4);
    ASSERT(abs(docs[0].relevance - 0.196166) <= RELEVANCE_THRESHOLD);
    ASSERT(abs(docs[1].relevance - 0.163471) <= RELEVANCE_THRESHOLD);
    ASSERT(abs(docs[2].relevance - 0.095894) <= RELEVANCE_THRESHOLD);
    ASSERT_EQUAL(docs[3].relevance, 0);
}

void TestGetDocumentId() {
    SearchServer server;
    server.AddDocument(1, "a b c d e", DocumentStatus::ACTUAL, {1, 2, 3, 4, 5});
    server.AddDocument(3, "a b c d e f", DocumentStatus::ACTUAL, {1, 2, 3, 4, 5});
    server.AddDocument(2, "a b c", DocumentStatus::ACTUAL, {10, 20, 30, 40, 50});
    server.AddDocument(5, "a a b", DocumentStatus::ACTUAL, {100, 200, 300, 400, 500});
    ASSERT_EQUAL(server.GetDocumentId(1), 3);
}

void TestSearchServer() {
    TestExcludeStopWordsFromAddedDocumentContent();
    TestMinusWords();
    TestDocumentMatching();
    TestSortByRelevance();
    TestCalcDocumentRating();
    TestUserFilterPredicate();
    TestDocumentStatusFilter();
    TestCalcRelevance();
    TestGetDocumentId();
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}