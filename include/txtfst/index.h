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
    std::vector<std::string> names; // store all the names
    std::vector<std::vector<size_t> > book_pathes; // store all the book pathes
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
          std::string path;
          for (auto&& name_idx : book_pathes[r.idx])
            path += names[name_idx] + "/";
          path.pop_back();
          ret.emplace_back(path);
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
        std::ranges::sort(sorted_books, std::greater{}, [](auto&& r) { return r.content_freq; });
        for (auto& r : sorted_books)
        {
          if (r.content_freq == 0) break;
          std::string path;
          for (auto&& name_idx : book_pathes[r.idx])
            path += names[name_idx] + "/";
          path.pop_back();
          ret.emplace_back(path);
        }
      }
      return ret;
    }
  };

  class IndexBuilder
  {
    std::vector<std::string> names;
    std::vector<std::vector<size_t> > book_pathes;
    std::vector<Entry> merged_entries;
    std::map<std::string, std::map<size_t, Entry::BookEntry> > unmerged_tokens;
    FSTBuilder<uint32_t> fst_builder;

  public:
    IndexBuilder& add_book(const std::string& path,
                           const std::vector<std::string>& title,
                           const std::vector<std::string>& content)
    {
      book_pathes.emplace_back();
      for (auto&& name : path | std::views::split('/'))
      {
        auto sv = std::string_view{name};
        auto it = std::ranges::find(names, sv);
        if (it == names.end())
        {
          names.emplace_back(sv);
          book_pathes.back().emplace_back(names.size() - 1);
        }
        else
          book_pathes.back().emplace_back(it - names.cbegin());
      }

      auto curr_book = book_pathes.size() - 1;
      for (auto&& token : title)
      {
        auto& curr_entry = unmerged_tokens[token];
        if (auto it = curr_entry.find(curr_book); it == curr_entry.end())
          curr_entry[curr_book] = Entry::BookEntry{curr_book, 1, 0};
        else
          ++it->second.title_freq;
      }

      for (auto&& token : content)
      {
        auto& curr_entry = unmerged_tokens[token];
        if (auto it = curr_entry.find(curr_book); it == curr_entry.end())
          curr_entry[curr_book] = Entry::BookEntry{curr_book, 0, 1};
        else
          ++it->second.content_freq;
      }
      return *this;
    }

    Index build()
    {
      for (auto&& r : unmerged_tokens)
      {
        fst_builder.add(r.first, merged_entries.size());
        std::vector<Entry::BookEntry> book_entries;
        for (auto&& t : r.second)
          book_entries.emplace_back(t.second);
        merged_entries.emplace_back(book_entries);
      }
      return Index{names, book_pathes, fst_builder.build(), merged_entries};
    }
  };
}
#endif
