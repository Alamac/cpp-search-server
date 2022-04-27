#include <stdexcept>
#include <numeric>
#include <algorithm>
#include <vector>
#include <cmath>
#include <iostream>
#include <optional>
#include <execution>
#include <chrono>
#include <thread>

#include "log_duration.h"
#include "search_server.h"
#include "string_processing.h"

using namespace std::string_literals;

SearchServer::SearchServer() {}

SearchServer::SearchServer(const std::string& text) {
    for (std::string_view word : SplitIntoWords(text)) {
        if (SearchServer::StringHasSpecialSymbols(word)) {
            throw std::invalid_argument("There is a special symbol in stopword: "s + std::string(word));
        }
        stop_words_.insert(std::string(word));
    }
}

SearchServer::SearchServer(std::string_view text) {
    for (std::string_view word : SplitIntoWords(text)) {
        if (SearchServer::StringHasSpecialSymbols(word)) {
            throw std::invalid_argument("There is a special symbol in stopword: "s + std::string(word));
        }
        stop_words_.insert(std::string(word));
    }
}

int SearchServer::GetDocumentCount() const {
    return SearchServer::documents_.size();
}

std::map<std::string_view, std::map<int, double>> SearchServer::GetWordToFreqs() const {
    return word_to_document_freqs_;
}

void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
    if (document_id < 0) {
        throw std::invalid_argument("Document id("s + std::to_string(document_id) + ") is less then 0"s);
    }
    if (documents_.find(document_id) != documents_.end()) {
        throw std::invalid_argument("There is already a document in document list with id: "s + std::to_string(document_id));
    }
    if (SearchServer::StringHasSpecialSymbols(document)) {
        throw std::invalid_argument("There is a special symbol in document: "s + std::string(document));
    }

    documents_[document_id].text = std::string(document);
    documents_[document_id].rating = ComputeAverageRating(ratings);
    documents_[document_id].status = status;

    const std::vector<std::string_view> words = SplitIntoWordsNoStop(documents_[document_id].text);
    const double inv_word_count = 1.0 / words.size();
    std::map<std::string_view, double> word_freqs;
    for (const std::string_view word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        word_freqs[word] += inv_word_count;
    }
    documents_[document_id].word_count = word_freqs;
    document_ids_.insert(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(std::execution::seq, raw_query, status);
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const {            
    return SearchServer::FindTopDocuments(std::execution::seq, raw_query);
}

using MatchDocumentResult = std::tuple<std::vector<std::string_view>, DocumentStatus>;
MatchDocumentResult SearchServer::MatchDocument(
    const std::execution::sequenced_policy&, std::string_view raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query, false);
        std::vector<std::string_view> matched_words;
        for (std::string_view word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                return {matched_words, documents_.at(document_id).status};
            }
        }
        for (std::string_view word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        return {matched_words, documents_.at(document_id).status};
}

MatchDocumentResult SearchServer::MatchDocument(std::string_view raw_query, int document_id) const {
    return MatchDocument(std::execution::seq, raw_query, document_id);
}

MatchDocumentResult SearchServer::MatchDocument(
    const std::execution::parallel_policy&, std::string_view raw_query, int document_id) const {
        Query query = ParseQuery(raw_query, true);

        const auto word_checker = 
            [this, document_id](std::string_view word) {
                const auto it = word_to_document_freqs_.find(word);
                return it != word_to_document_freqs_.end() && it -> second.count(document_id);
            };

        if (std::any_of(query.minus_words.begin(), query.minus_words.end(), word_checker)) {
            return {{}, documents_.at(document_id).status};
        }

        std::vector<std::string_view> matched_words(query.plus_words.size());
        auto words_end = copy_if(
            std::execution::par,
            query.plus_words.begin(), query.plus_words.end(),
            matched_words.begin(),
            word_checker
        );
        std::sort(std::execution::par, matched_words.begin(), words_end);
        words_end = std::unique(matched_words.begin(), words_end);
        matched_words.erase(words_end, matched_words.end());
        return {matched_words, documents_.at(document_id).status};
}

std::set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

std::set<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    if (document_ids_.count(document_id) > 0) {
        return documents_.at(document_id).word_count;
    }
    static const std::map<std::string_view, double> empty_map = {};
    return empty_map;
}

void SearchServer::RemoveDocument(int document_id) {
    if (document_ids_.count(document_id) == 0) {
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
    document_ids_.erase(document_id);
}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy& policy, int document_id) {
    RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy& policy, int document_id) {
    if (document_ids_.count(document_id) == 0) {
        return;
    }
    auto words = documents_.at(document_id).word_count;
    std::vector<const std::string_view*> tmp_vector;
    tmp_vector.reserve(words.size());
    std::transform(policy, 
        words.begin(), 
        words.end(),
        tmp_vector.begin(),
        [&](const auto &pair){
            return &pair.first;
        });
    
    std::for_each(policy, 
        tmp_vector.begin(), 
        tmp_vector.end(), 
        [this, document_id](const auto &key) {
            word_to_document_freqs_[*key].erase(document_id);
        }
    );
    documents_.erase(document_id);
    document_ids_.erase(document_id);
}

bool SearchServer::IsStopWord(std::string_view word) const {
    return stop_words_.count(std::string(word)) > 0;
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const {
    std::vector<std::string_view> words;
    for (std::string_view word : SplitIntoWords(text)) {
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
    if (text.empty()) {
        throw std::invalid_argument("Empty query word");
    }
    bool is_minus = false;
    if (text[0] == '-') {
        is_minus = true;
        text.remove_prefix(1);
    }
    return {
        text,
        is_minus,
        IsStopWord(text)
    };
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text, bool skip_sort) const {
    Query query;
    for (std::string_view word : SplitIntoWords(text)) {
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
                query.minus_words.push_back(query_word.data);
            } else {
                query.plus_words.push_back(query_word.data);
            }
        }
    }
    if (!skip_sort) {
        for (auto* words : {&query.plus_words, &query.minus_words}) {
            std::sort(words->begin(), words->end());
            words->erase(std::unique(words->begin(), words->end()), words->end());
        }
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

void SearchServer::ApplyMaxResultDocumentCount(std::vector<Document>& docs) {
    if (docs.size() > MAX_RESULT_DOCUMENT_COUNT) {
        docs.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

bool SearchServer::StringHasSpecialSymbols(std::string_view s) const {
    std::string tmp_str = static_cast<std::string>(s);
    for (const auto& c : s) {
            if (int(c) <= 31 && int(c) >= 0) {
                return true;
            }
        }
    return false;
}