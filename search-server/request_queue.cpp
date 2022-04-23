#include "request_queue.h"


RequestQueue::RequestQueue(const SearchServer& search_server){
    server_ = &search_server;
    empty_requests_ = 0;
    time_ = 0;
}

std::vector<Document> RequestQueue::AddFindRequest(std::string_view raw_query, DocumentStatus status) {
    std::vector<Document> search_result = server_->FindTopDocuments(raw_query, status);
    ProcessRequest(search_result);
    return search_result;
}

std::vector<Document> RequestQueue::AddFindRequest(std::string_view raw_query) {
    std::vector<Document> search_result = server_->FindTopDocuments(raw_query);
    ProcessRequest(search_result);
    return search_result;
}

int RequestQueue::GetNoResultRequests() const {
    return empty_requests_;
}

void RequestQueue::ProcessRequest(const std::vector<Document>& search_result) {
    QueryResult result = {search_result, search_result.empty()};
    ++time_;
    if (time_ > min_in_day_ && !requests_.empty()) {
        RemoveRequest();
    }
    AddRequest(result);
}

void RequestQueue::RemoveRequest() {
    QueryResult req = requests_.front();
    if (req.is_empty) {
        --empty_requests_;
    }
    requests_.pop_front();
}

void RequestQueue::AddRequest(const QueryResult& request) {
    if (request.is_empty) {
        ++empty_requests_;
    }
    requests_.push_back(request);
}