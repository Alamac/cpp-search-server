#include "paginator.h"
#include "request_queue.h"
#include "search_server.h"
#include "module_tests.h"


using namespace std;

// TEST FRAMEWORK

//END OF FRAMEWORK

// MODULE TESTS
//END OF MODULE TESTS

int main() {
    TestSearchServer();
    std::cout << "Module tests completed successfully!"s << std::endl;
    return 0;
}