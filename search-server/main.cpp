#include "paginator.h"
#include "request_queue.h"
#include "search_server.h"
#include "test_example_functions.h"
#include "remove_duplicates.h"

using namespace std::string_literals;

int main() {
    TestSearchServer();
    std::cout << "Module tests completed successfully!"s << std::endl;
    return 0;
}