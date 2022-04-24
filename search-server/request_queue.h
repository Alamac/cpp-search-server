#pragma once

#include <vector>
#include <deque>
#include <string>
#include <execution>

#include "search_server.h"


class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(std::string_view raw_query, DocumentPredicate document_predicate);

    std::vector<Document> AddFindRequest(std::string_view raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(std::string_view raw_query);

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        std::vector<Document> documents;
        bool is_empty;
    };
    std::deque<QueryResult> requests_;
    int empty_requests_;
    uint64_t time_;
    const static int min_in_day_ = 1440;
    const SearchServer* server_;

    void ProcessRequest(const std::vector<Document>& search_result);

    void RemoveRequest();

    void AddRequest(const QueryResult& request);

};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(std::string_view raw_query, DocumentPredicate document_predicate) {
    std::vector<Document> search_result = server_->FindTopDocuments(raw_query, document_predicate);
    RequestQueue::ProcessRequest(search_result);
    return search_result;
}