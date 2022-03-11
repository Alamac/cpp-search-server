#pragma once

#include <vector>
#include <set>
#include <string>
#include <map>
#include <stdexcept>

#include "document.h"
#include "string_processing.h"

using namespace std::string_literals;

class SearchServer {
public:

    static constexpr double RELEVANCE_THRESHOLD = 1e-6;
    static const int MAX_RESULT_DOCUMENT_COUNT = 5;

    SearchServer();

    SearchServer(const std::string& text);

    template<typename C, typename T = typename C::value_type>
    SearchServer(const C& container);

    int GetDocumentCount() const;

    std::map<std::string, std::map<int, double>> GetWordToFreqs() const;
    
    void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);

    template<typename Filter>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, Filter FilterLamdaFunc) const;

    //to use with DocumentStatus
    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus status) const;

    //to use without second arg at all (using DocumentStatus::ACTUAL)
    std::vector<Document> FindTopDocuments(const std::string& raw_query) const;
    
    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;

    int GetDocumentId(int index) const;

    std::set<int>::const_iterator begin() const;

    std::set<int>::const_iterator end() const;

    const std::map<std::string, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);
private:
    //structs
    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

    struct QueryWord {
        std::string data;
        bool is_minus;
        bool is_stop;
    };

    struct DocumentData {
    int rating;
    DocumentStatus status;
    std::map<std::string, double> word_count;
    };

    //vars
    std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;
    
    //methods
    bool IsStopWord(const std::string& word) const;
    
    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;
    
    QueryWord ParseQueryWord(std::string text) const;
    
    Query ParseQuery(const std::string& text) const;
    
    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    std::vector<Document> FindAllDocuments(const Query& query) const;

    bool StringHasSpecialSymbols(const std::string& s) const;

    //static methods
    static void SortDocuments(std::vector<Document>& documents);

    static void ApplyMaxResultDocumentCount(std::vector<Document>& docs);

    static int ComputeAverageRating(const std::vector<int>& ratings);
};

template<typename C, typename T>
SearchServer::SearchServer(const C& container) {
    for (const std::string& word : container) {
        if (SearchServer::StringHasSpecialSymbols(word)) {
            throw std::invalid_argument("There is a special symbol in stopword: "s + word);
        }
        if (word != ""s) {
            stop_words_.insert(word);
        }
    }
}

//to use with filter lambda
template<typename Filter>
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, Filter FilterLamdaFunc) const {            
    const Query query = ParseQuery(raw_query);
    std::vector<Document> matched_documents = FindAllDocuments(query);
    
    SortDocuments(matched_documents);

    std::vector<Document> filtered_documents;

    for (const auto& doc : matched_documents) {
        if (FilterLamdaFunc(doc.id, documents_.at(doc.id).status, documents_.at(doc.id).rating)) {
            filtered_documents.push_back(doc);
        }
    }

    ApplyMaxResultDocumentCount(filtered_documents);
    return filtered_documents;
}