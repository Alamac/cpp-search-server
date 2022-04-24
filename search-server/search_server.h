#pragma once

#include <vector>
#include <set>
#include <unordered_set>
#include <string>
#include <map>
#include <stdexcept>
#include <execution>
#include <string_view>
#include <future>
#include <iterator>
#include <type_traits>

#include "document.h"
#include "string_processing.h"
#include "log_duration.h"

using namespace std;
 
template <typename ExecutionPolicy, typename ForwardRange, typename Function>
void ForEach(const ExecutionPolicy& policy, ForwardRange& range, Function function) {
    
        static constexpr int PART_COUNT = 4;
        const auto part_length = size(range) / PART_COUNT;
        auto part_begin = range.begin();
        auto part_end = next(part_begin, part_length);
 
        vector<future<void>> futures;
        for (int i = 0;
             i < PART_COUNT;
             ++i,
                 part_begin = part_end,
                 part_end = (i == PART_COUNT - 1
                                 ? range.end()
                                 : next(part_begin, part_length))
             ) {
            futures.push_back(async([function, part_begin, part_end] {
                for_each(part_begin, part_end, function);
            }));
        }
}
 
template <typename ForwardRange, typename Function>
void ForEach(ForwardRange& range, Function function) {
    ForEach(execution::seq, range, function);
}

using namespace std::string_literals;
class SearchServer {
public:

    static constexpr double RELEVANCE_THRESHOLD = 1e-6;
    static const int MAX_RESULT_DOCUMENT_COUNT = 5;

    SearchServer();

    SearchServer(const std::string& text);
    SearchServer(std::string_view text);

    template<typename C, typename T = typename C::value_type>
    SearchServer(const C& container);

    int GetDocumentCount() const;

    std::map<std::string_view, std::map<int, double>> GetWordToFreqs() const;
    
    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    template<typename Filter>
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy& policy, std::string_view raw_query, Filter FilterLambdaFunc) const;

    template<typename Filter>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, Filter FilterLambdaFunc) const;

    template<typename Filter>
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy& policy,std::string_view raw_query, Filter FilterLambdaFunc) const;

    //to use with DocumentStatus
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy& policy, std::string_view raw_query, DocumentStatus status) const;

    //to use without second arg at all (using DocumentStatus::ACTUAL)
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy& policy, std::string_view raw_query) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

    //to use with DocumentStatus
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy& policy, std::string_view raw_query, DocumentStatus status) const;

    //to use without second arg at all (using DocumentStatus::ACTUAL)
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy& policy, std::string_view raw_query) const;
    
    
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
        const std::execution::sequenced_policy&, std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
        const std::execution::parallel_policy&, std::string_view raw_query, int document_id) const;

    int GetDocumentId(int index) const;

    std::set<int>::const_iterator begin() const;

    std::set<int>::const_iterator end() const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);
    void RemoveDocument(const std::execution::sequenced_policy& policy, int document_id);
    void RemoveDocument(const std::execution::parallel_policy& policy, int document_id);
private:
    //structs
    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    struct DocumentData {
    int rating;
    DocumentStatus status;
    std::map<std::string_view, double> word_count;
    std::string text;
    };

    //vars
    std::set<std::string> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;
    
    //methods
    bool IsStopWord(const std::string_view word) const;
    
    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;
    
    QueryWord ParseQueryWord(std::string_view text) const;
    
    Query ParseQuery(std::string_view text, bool skip_sort = true) const;
    
    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy& policy, const Query& query) const;
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy& policy, const Query& query) const;

    bool StringHasSpecialSymbols(std::string_view s) const;

    //static methods
    static void SortDocuments(const std::execution::sequenced_policy& policy, std::vector<Document>& documents);

    static void SortDocuments(const std::execution::parallel_policy& policy, std::vector<Document>& documents);

    static void ApplyMaxResultDocumentCount(std::vector<Document>& docs);

    static int ComputeAverageRating(const std::vector<int>& ratings);
};

template<typename C, typename T>
SearchServer::SearchServer(const C& container) {
    TransparentStringSet stop_words = MakeUniqueNonEmptyStrings(container);
    for (std::string_view word : stop_words) {
        if (SearchServer::StringHasSpecialSymbols(word)) {
            throw std::invalid_argument("There is a special symbol in stopword: "s + std::string(word));
        }
        stop_words_.insert(std::string(word));
    }
}

//to use with filter lambda
template<typename Filter>
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy& policy, std::string_view raw_query, Filter FilterLambdaFunc) const {            
    const Query query = ParseQuery(raw_query, false);
    std::vector<Document> matched_documents = FindAllDocuments(policy, query);
    
    SortDocuments(policy, matched_documents);

    std::vector<Document> filtered_documents;

    for (const auto& doc : matched_documents) {
        if (FilterLambdaFunc(doc.id, documents_.at(doc.id).status, documents_.at(doc.id).rating)) {
            filtered_documents.push_back(doc);
        }
    }

    ApplyMaxResultDocumentCount(filtered_documents);
    return filtered_documents;
}

template<typename Filter>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, Filter FilterLambdaFunc) const {            
    return FindTopDocuments(std::execution::seq, raw_query, FilterLambdaFunc);
}

template<typename Filter>
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy& policy, std::string_view raw_query, Filter FilterLambdaFunc) const {          
    const Query query = ParseQuery(raw_query, false);
    std::vector<Document> matched_documents = FindAllDocuments(policy, query);

    SortDocuments(policy, matched_documents);

    std::vector<Document> filtered_documents;
    filtered_documents.reserve(matched_documents.size());
    
    /*ForEach(policy, matched_documents, [] (const auto& doc) {
        if (FilterLambdaFunc(doc.id, documents_.at(doc.id).status, documents_.at(doc.id).rating)) {
            filtered_documents.push_back(doc);
         }
    });*/
    for (const auto& doc : matched_documents) {
        if (FilterLambdaFunc(doc.id, doc.status, doc.rating)) {
            filtered_documents.push_back(doc);
        }
    }
    
    ApplyMaxResultDocumentCount(filtered_documents);
    return filtered_documents;
}