#pragma once

#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>

using namespace std::string_literals;

template <typename I>
void PrintRange(I first, I last, std::ostream& os) {
    for (auto it = first; it != last; ++it) {
        os << *it;
    }
}

template <typename It>
class IteratorRange {
public:
    IteratorRange(){};

    IteratorRange(It begin, It end) {
        begin_ = begin;
        end_ = end;
    }
    IteratorRange(It begin, size_t size) {
        begin_ = begin;
        end_ = std::advance(begin, size);
    }

    It begin() const {
        return begin_;
    }
    It end() const {
        return end_;
    }

    auto size() {
        return distance(begin_, end_);
    }

private:
    It begin_;
    It end_;
};

template <typename Iterator>
std::ostream& operator<<(std::ostream& os, const IteratorRange<Iterator>& range) {
    PrintRange(range.begin(), range.end(), os);
    return os;
}

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator start, Iterator end, size_t size) {
        if (start == end) {
            throw std::invalid_argument("Can't use empty container for Paginator class initialization"s);
        }
        if (size <= 0) {
            throw std::invalid_argument("Can't paginate with zero or negative size"s);
        }
        while (start < end) {
            Iterator start_position = start;
            std::advance(start, size);
            if (start < end) {
                pages_.push_back(IteratorRange<Iterator>(start_position, start));
            }
            else {
                pages_.push_back(IteratorRange<Iterator>(start_position, end));
            }
        }
    }
    auto begin() const {
        return pages_.begin();
    }
    auto end() const {
        return pages_.end();
    }
    
    auto size() const {
        return pages_.size();
    }

private:
    std::vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}