#pragma once

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

     explicit ConcurrentMap(size_t bucket_count) {
          for (int i = 0; i < static_cast<int>(bucket_count); ++i) {
               data_[i];
               //mutexes_vctr_.push_back(std::mutex());
               ++size_;
          }
          //mutexes_vctr_(size_, std::mutex());
          mutexes_vctr_ = std::vector<std::mutex>(size_);
     }

    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;
    };

    Access operator[](const Key& key) {
         int maps_index = static_cast<uint64_t>(key) % size_;
         return { std::lock_guard(mutexes_vctr_[maps_index]), data_[maps_index][key] };
    }

    std::map<Key, Value> BuildOrdinaryMap() {
         std::map<Key, Value> united_map;
         
         for (int i = 0; i < size_; ++i) {
              std::lock_guard guardian (mutexes_vctr_[i]);
              united_map.insert(data_[i].begin(), data_[i].end());
         }
         return united_map;
    }

private:
    std::map<int, std::map<Key, Value>> data_;
    std::vector<std::mutex> mutexes_vctr_;
    int size_ = 0;
};