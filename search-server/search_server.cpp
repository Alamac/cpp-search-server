#include <stdexcept>
#include <numeric>
#include <algorithm>
#include <vector>
#include <cmath>
#include <iostream>
#include <optional>

#include "log_duration.h"
#include "search_server.h"
#include "string_processing.h"

using namespace std::string_literals;

SearchServer::SearchServer() {}

SearchServer::SearchServer(const std::string& text) {
    for (const std::string& word : SplitIntoWords(text)) {
    if (SearchServer::StringHasSpecialSymbols(word)) {
        throw std::invalid_argument("There is a special symbol in stopword: "s + word);
    }
    stop_words_.insert(word);
    }
}

int SearchServer::GetDocumentCount() const {
    return SearchServer::documents_.size();
}

std::map<std::string, std::map<int, double>> SearchServer::GetWordToFreqs() const {
    return word_to_document_freqs_;
}

void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {
    if (document_id < 0) {
        throw std::invalid_argument("Document id("s + std::to_string(document_id) + ") is less then 0"s);
    }
    if (documents_.find(document_id) != documents_.end()) {
        throw std::invalid_argument("There is already a document in document list with id: "s + std::to_string(document_id));
    }
    if (SearchServer::StringHasSpecialSymbols(document)) {
        throw std::invalid_argument("There is a special symbol in document: "s + document);
    }

    const std::vector<std::string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    std::map<std::string, double> word_freqs;
    for (const std::string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        word_freqs[word] += inv_word_count;
    }
    documents_.emplace(
        document_id, 
        DocumentData{
            ComputeAverageRating(ratings), 
            status,
            word_freqs
        });
    document_ids_.insert(document_id);
}

//to use with DocumentStatus
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus status) const {
    return SearchServer::FindTopDocuments(raw_query, [status](int doc_id, DocumentStatus doc_status, int doc_rating) 
        {
            return doc_status == status;
        }
    );
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const {            
    return SearchServer::FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {
    const Query query = ParseQuery(raw_query);
    std::vector<std::string> matched_words;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const std::string& word : query.minus_words) {
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

/* DEPRECATED
int SearchServer::GetDocumentId(int index) const {
    if (index >= SearchServer::documents_.size()) {
        throw std::out_of_range("Document list size is less then "s + std::to_string(index));
    }
    else {
        auto it = documents_.begin();
        std::advance(it, index);
        return it->first;
    }
}
*/

std::set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

std::set<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

const std::map<std::string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    if (documents_.count(document_id) > 0) {
        return documents_.at(document_id).word_count;
    }
    static const std::map<std::string, double> empty_map = {};
    return empty_map;
}

void SearchServer::RemoveDocument(int document_id) {
    if (documents_.count(document_id) == 0) {
        return;
    }
    for (auto [key, value] : documents_.at(document_id).word_count) {
        if (word_to_document_freqs_[key].size() == 1) {
            word_to_document_freqs_.erase(key);
        }
        else {
            word_to_document_freqs_[key].erase(document_id);
        }
    }
    documents_.erase(document_id);
    auto position = find(document_ids_.begin(), document_ids_.end(), document_id);
    document_ids_.erase(position);
}

bool SearchServer::IsStopWord(const std::string& word) const {
    return stop_words_.count(word) > 0;
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
    std::vector<std::string> words;
    for (const std::string& word : SplitIntoWords(text)) {
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string text) const {
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

SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
    Query query;
    for (const std::string& word : SplitIntoWords(text)) {
            if (word[0] == '-' && word[1] == '-') {
            throw std::invalid_argument("There is a word with double minus(--) in the search query");
        }
        if (word[0] == '-' && word.size() == 1) {
            throw std::invalid_argument("There is a single minus( - ) in the search query");
        }
        if (SearchServer::StringHasSpecialSymbols(word)) {
            throw std::invalid_argument("There is a special symbol in the search query");
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

double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

std::vector<Document> SearchServer::FindAllDocuments(const Query& query) const {
    std::map<int, double> document_to_relevance;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            document_to_relevance[document_id] += term_freq * inverse_document_freq;
        }
    }
    
    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({
            document_id,
            relevance,
            documents_.at(document_id).rating
        });
    }
    return matched_documents;
}

void SearchServer::SortDocuments(std::vector<Document>& documents) {
    sort(documents.begin(), documents.end(),
            [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < SearchServer::RELEVANCE_THRESHOLD) {
                return lhs.rating > rhs.rating;
            } else {
                return lhs.relevance > rhs.relevance;
            }
            });
}

void SearchServer::ApplyMaxResultDocumentCount(std::vector<Document>& docs) {
    if (docs.size() > SearchServer::MAX_RESULT_DOCUMENT_COUNT) {
        docs.resize(SearchServer::MAX_RESULT_DOCUMENT_COUNT);
    }
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

bool SearchServer::StringHasSpecialSymbols(const std::string& s) const {
    for (const auto& c : s) {
            if (int(c) <= 31 && int(c) >= 0) {
                return true;
            } 
        }
    return false;
}