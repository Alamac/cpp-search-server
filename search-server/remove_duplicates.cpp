#include <iostream>

#include "remove_duplicates.h"

using namespace std::string_literals;

void RemoveDuplicates(SearchServer& search_server) {
    std::set<std::set<std::string_view>> all_words;
    std::vector<int> dublicates;
    for (const int doc_id : search_server){
        auto doc_word_freqs = search_server.GetWordFrequencies(doc_id);
        std::set<std::string_view> words;
        for (auto word_freqs : doc_word_freqs) {
            words.insert(word_freqs.first);
        }
        if (all_words.count(words) == 0) {
            all_words.insert(words);
        } 
        else {
        dublicates.push_back(doc_id);
            std::cout << "Found duplicate document id "s << doc_id << std::endl;
        }
 
    }
    for (int doc_id : dublicates) {
        search_server.RemoveDocument(doc_id);
    }
}