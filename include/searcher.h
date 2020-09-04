#pragma once

#include <string>
#include <utility>
#include <unordered_map>
#include <vector>
#include <map>
#include <set>
#include <sstream>
#include <bits/stl_algo.h>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <memory>

class Searcher
{
private:
    std::map<std::string, std::map<std::string, std::set<size_t>>> inverted_index;
    std::map<std::string, std::vector<std::string>> text;
public:
    using Filename = std::string;


    void add_document(const Filename & filename, std::istream & strm) {
        std::string current;
        int ind = 0;
        while (strm >> current) {
            std::string new_string;
            size_t pos = 0;
            while (pos < current.size() && ispunct(current[pos])) {
                pos++;
            }
            while (pos < current.size()) {
                new_string.push_back(current[pos]);
                pos++;
            }
            while (ispunct(new_string.back())) {
                new_string.pop_back();
            }
            text[filename].push_back(new_string);
            inverted_index[new_string][filename].insert(ind);
            ind++;
        }
    }

    void remove_document(const Filename & filename) {
        size_t ind = 0;
        for (const auto& str : text[filename]) {
            auto it = inverted_index[str][filename].find(ind);
            inverted_index[str][filename].erase(it);
            if (inverted_index[str][filename].empty()) {
                inverted_index[str].erase(filename);
            }
            if (inverted_index[str].empty()) {
                inverted_index.erase(str);
            }
            ind++;
        }
        text.erase(filename);
    }

    class iterator;
    class DocIterator
    {
        friend class Searcher;
        std::shared_ptr<std::set<std::string>> my_set;
        std::set<std::string>::iterator it;
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef ptrdiff_t difference_type;
        typedef const std::string value_type;
        typedef value_type *pointer;
        typedef value_type &reference;

    public:
        DocIterator() = default;

        DocIterator(DocIterator const &other) = default;

    private:
        explicit DocIterator(const std::set<std::string>::iterator it, std::shared_ptr<std::set<std::string>> my_set) : my_set(std::move(my_set)), it(it) {}

    public:
        reference operator*() const {
            return it.operator*();
        }

        pointer operator->() const {
            return it.operator->();
        }

        DocIterator &operator++() {
            it++;
            return *this;
        }

        DocIterator operator++(int) {
            auto ans = *this;
            ++*this;
            return ans;
        }

        friend bool operator==(const DocIterator &it1, const DocIterator &it2) {
            return it1.it == it2.it;
        }

        friend bool operator!=(const DocIterator &it1, const DocIterator &it2) {
            return it1.it != it2.it;
        }
    };


    class BadQuery : public std::exception
    {
    public:
        explicit BadQuery(const std::string& message) {
            this->message = message;
        }
        const char * what() const noexcept override {
            return message.c_str();
        }
    private:
        std::string message;
    };

    void parse_str(const std::string& query, std::vector<std::vector<std::string>> & words) const {
        std::istringstream ss(query);
        std::string curr;
        std::vector<std::string> buf;
        bool phrase = false;
        while (ss >> curr) {
            if (curr[0] == '"' and curr[curr.size() - 1] != '"') {
                phrase = true;
            }
            std::string token;
            size_t i = 0;
            while (i < curr.size() && ispunct(curr[i])) {
                i++;
            }
            while (i < curr.size()) {
                token.push_back(curr[i]);
                i++;
            }
            while (ispunct(token.back())) {
                token.pop_back();
            }
            if (token.empty()) {
                continue;
            }
            buf.push_back(token);
            if (curr[curr.size() - 1] == '"' && !phrase) {
                throw BadQuery("no quotes!");
            }
            if ((curr[curr.size() - 1] == '"' && phrase) || !phrase) {
                words.push_back(buf);
                buf.clear();
                phrase = false;
            }
        }
        if (phrase) {
            throw BadQuery("incorrect input!");
        }
        if (words.empty()) {
            throw BadQuery("no words in input");
        }
    }

    void get_phrase(std::vector<std::string> & phrase, std::set<std::string> & result) const {

        std::set<std::string> curr_docs;
        auto it1 = inverted_index.find(phrase[0]);
        if (it1 == inverted_index.end()) {
            return;
        }
        for (auto & doc : (*it1).second) {
            curr_docs.insert(doc.first);
        }

        for (size_t i = 1; i < phrase.size(); i++) {
            it1 = inverted_index.find(phrase[i]);
            if (it1 == inverted_index.end()) {
                return;
            }
            std::set<std::string> next_docs;
            for (auto & d : (*it1).second) {
                next_docs.insert(d.first);
            }
            std::set<std::string> temp;
            std::set_intersection(curr_docs.begin(), curr_docs.end(), next_docs.begin(), next_docs.end(), std::inserter(temp, temp.begin()));
            curr_docs = temp;
        }

        for (auto & doc : curr_docs) {
            it1 = inverted_index.find(phrase[0]);
            auto it2 = text.find(doc);
            auto current_docs = (*((*it1).second.find(doc))).second;
            for (size_t word = 1; word < phrase.size(); word++) {
                std::set<size_t> next;
                for (size_t pos : current_docs) {
                    if (++pos < (*it2).second.size() && (*it2).second[pos] == phrase[word]) {
                        next.insert(pos);
                    }
                }
                current_docs = next;
            }
            if (!current_docs.empty()) {
                result.insert(doc);
            }
        }

    }

    std::pair<DocIterator, DocIterator> search(const std::string & query) const {
        std::vector<std::string> res;
        std::vector<std::vector<std::string>> words;
        auto docs = std::shared_ptr<std::set<std::string>>();
        std::set<std::string> current_docs;
        parse_str(query, words);

        if (words[0].size() == 1) {
            auto it1 = inverted_index.find(words[0][0]);
            if (it1 == inverted_index.end()) {
                docs = std::make_shared<std::set<std::string>>(current_docs.begin(), current_docs.end());
                return std::pair<DocIterator, DocIterator>(DocIterator(docs->begin(), docs), DocIterator(docs->end(), docs));
            }
            for (const auto& next : (*it1).second) {
                current_docs.insert(next.first);
            }
        } else {
            get_phrase(words[0], current_docs);

        }
        for (size_t w = 1; w < words.size(); w++) {
            std::set<std::string> temp;
            std::set<std::string> curr;
            if (words[w].size() == 1) {
                auto it1 = inverted_index.find(words[w][0]);
                if (it1 == inverted_index.end()) {
                    current_docs.clear();
                    break;
                }
                for (auto & str : (*it1).second) {
                    curr.insert(str.first);
                }
                std::set_intersection(current_docs.begin(), current_docs.end(), curr.begin(), curr.end(), std::inserter(temp, temp.begin()));
            } else {
                get_phrase(words[w], curr);
                std::set_intersection(current_docs.begin(), current_docs.end(), curr.begin(), curr.end(), std::inserter(temp, temp.begin()));
            }
            current_docs = temp;
        }
        docs = std::make_shared<std::set<std::string>>(current_docs.begin(), current_docs.end());
        return std::pair<DocIterator, DocIterator> (DocIterator(docs->begin(), docs), DocIterator(docs->end(), docs));
    }
};
