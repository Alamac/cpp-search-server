#include <algorithm>
#include <execution>

#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
        std::vector<std::vector<Document>> result(queries.size());
        std::transform(std::execution::par, queries.begin(), queries.end(), result.begin(), [&search_server](const auto& query) {
            return search_server.FindTopDocuments(std::execution::par, query);
        });
        return result;
    }

std::vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
        std::vector<Document> result(MAX_RESULT_DOCUMENT_COUNT * queries.size());
        auto mid_result = ProcessQueries(search_server, queries);
        int i = 0;
        for (auto& v : mid_result) {
            std::move(v.begin(), v.end(), result.begin() + i);
            i += v.size();
        }
        result.resize(i);
        return result;
    }