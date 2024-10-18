#ifndef TXTFST_INDEX_H
#define TXTFST_INDEX_H
#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <ranges>
#include <algorithm>

#include "fst.h"

namespace txtfst
{
  struct Entry
  {
    struct BookEntry
    {
      size_t idx{0};
      size_t title_freq{0};
      size_t content_freq{0};
    };

    std::vector<BookEntry> books;
  };

  struct Index
  {
    std::vector<std::string> book_pathes; // store all the book pathes
    FST<uint32_t> fst; // store all the tokens
    std::vector<Entry> entries; // unique to a token

    [[nodiscard]] std::vector<std::string> search_title(const std::string& token) const
    {
      std::vector<std::string> ret;
      if (auto opt = fst.get(token); opt.has_value())
      {
        auto sorted_books = entries[*opt].books;
        std::ranges::sort(sorted_books, std::less{}, [](auto&& r) { return r.title_freq; });
        for (auto& r : sorted_books)
        {
          if (r.title_freq == 0) break;
          ret.emplace_back(book_pathes[r.idx]);
        }
      }
      return ret;
    }

    [[nodiscard]] std::vector<std::string> search_content(const std::string& token) const
    {
      std::vector<std::string> ret;
      if (auto opt = fst.get(token); opt.has_value())
      {
        auto sorted_books = entries[*opt].books;
        std::ranges::sort(sorted_books, std::less{}, [](auto&& r) { return r.content_freq; });
        for (auto& r : sorted_books)
        {
          if (r.content_freq == 0) break;
          ret.emplace_back(book_pathes[r.idx]);
        }
      }
      return ret;
    }
  };

  class IndexBuilder
  {
    std::vector<std::string> book_pathes;
    std::vector<Entry> merged_entries;
    std::map<std::string, Entry> unmerged_tokens;
    FSTBuilder<uint32_t> fst_builder;

  public:
    IndexBuilder& add_book(const std::string& path,
                           const std::vector<std::string>& title,
                           const std::vector<std::string>& content)
    {
      book_pathes.emplace_back(path);
      auto curr_book = book_pathes.size() - 1;
      for (auto&& token : title)
      {
        auto& books = unmerged_tokens[token].books;
        auto it = std::ranges::find(books, curr_book,
                                    [](auto&& b) { return b.idx; });
        if (it == books.cend())
          books.emplace_back(curr_book, 1, 0);
        else
          ++it->title_freq;
      }

      for (auto&& token : content)
      {
        auto& books = unmerged_tokens[token].books;
        auto it = std::ranges::find(books, curr_book,
                                    [](auto&& b) { return b.idx; });
        if (it == books.cend())
          books.emplace_back(curr_book, 0, 1);
        else
          ++it->content_freq;
      }
      return *this;
    }

    Index build()
    {
      for (auto&& r : unmerged_tokens)
      {
        fst_builder.add(r.first, merged_entries.size());
        merged_entries.emplace_back(r.second);
      }
      return Index{book_pathes, fst_builder.build(), merged_entries};
    }
  };
}
#endif
