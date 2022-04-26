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
#include "concurrent_map.h"

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

    template<typename Filter, typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query, Filter predicate) const;

    template<typename Filter>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, Filter predicate) const;

    //to use with DocumentStatus
    template<typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query, DocumentStatus status) const;

    //to use without second arg at all (using DocumentStatus::ACTUAL)
    template<typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;
    
    using MatchDocumentResult = std::tuple<std::vector<std::string_view>, DocumentStatus>;
    MatchDocumentResult MatchDocument(const std::execution::sequenced_policy&, std::string_view raw_query, int document_id) const;
    MatchDocumentResult MatchDocument(std::string_view raw_query, int document_id) const;
    MatchDocumentResult MatchDocument(const std::execution::parallel_policy&, std::string_view raw_query, int document_id) const;

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

    template <typename Filter>
    std::vector<Document> FindAllDocuments(const Query& query, Filter predicate) const;
    template <typename ExecutionPolicy, typename Filter>
    std::vector<Document> FindAllDocuments(const ExecutionPolicy& policy, const Query& query, Filter predicate) const;

    bool StringHasSpecialSymbols(std::string_view s) const;

    //static methods
    template <typename ExecutionPolicy>
    static void SortDocuments(const ExecutionPolicy& policy, std::vector<Document>& documents);

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

template<typename ExecutionPolicy>
void SearchServer::SortDocuments(const ExecutionPolicy& policy, std::vector<Document>& documents) {
    sort(policy, documents.begin(), documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (std::abs(lhs.relevance - rhs.relevance) < SearchServer::RELEVANCE_THRESHOLD) {
                    return lhs.rating > rhs.rating;
                } else {
                    return lhs.relevance > rhs.relevance;
                }
            });
}

template<typename Filter>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, Filter predicate) const {
    return FindAllDocuments(std::execution::seq, query, predicate);
}

template<typename ExecutionPolicy, typename Filter>
std::vector<Document> SearchServer::FindAllDocuments(const ExecutionPolicy& policy, const Query& query, Filter predicate) const {
    ConcurrentMap<int, double> document_to_relevance(document_ids_.size());

    std::for_each(policy, query.plus_words.begin(), query.plus_words.end(), [&](std::string_view word) {
        if (word_to_document_freqs_.count(word) > 0) {
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& doc_info = documents_.at(document_id);
                if (predicate(document_id, doc_info.status, doc_info.rating)) {
                    document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                }
            }
        }
    });

    std::map<int, double> document_map = document_to_relevance.BuildOrdinaryMap();

    std::for_each(policy, query.minus_words.begin(), query.minus_words.end(), [&](std::string_view word){
        if (word_to_document_freqs_.count(word) > 0) {
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_map.erase(document_id);
            }
        }
    });
    
    std::vector<Document> matched_documents(document_map.size());
    
    std::transform(document_map.begin(), document_map.end(), matched_documents.begin(), [&](const auto& it) {
        auto& i = documents_.at(it.first);
        return Document(it.first, it.second, i.rating, i.status);
    });

    return matched_documents;
}

//to use with filter lambda

template<typename Filter>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, Filter predicate) const {            
    return FindTopDocuments(std::execution::seq, raw_query, predicate);
}

template<typename Filter, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query, Filter predicate) const {            
    const Query query = ParseQuery(raw_query, false);
    std::vector<Document> matched_documents = FindAllDocuments(policy, query, predicate);
    SortDocuments(policy, matched_documents);
    ApplyMaxResultDocumentCount(matched_documents);
    return matched_documents;
}

template<typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query, DocumentStatus status) const {
    return SearchServer::FindTopDocuments(policy, raw_query, [status](int doc_id, DocumentStatus doc_status, int doc_rating) 
        {
            return doc_status == status;
        }
    );
}

template<typename ExecutionPolicy>
    std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query) const {            
        return SearchServer::FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}