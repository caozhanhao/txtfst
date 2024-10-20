#ifndef TXTFST_INDEX_H
#define TXTFST_INDEX_H
#pragma once

#include <string>
#include <vector>
#include <ranges>
#include <algorithm>
#include <map>

#include "packme/packme.h"
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

  struct IndexView
  {
    std::vector<std::string> names;
    std::vector<std::vector<size_t> > book_pathes;
    CompiledFSTView<uint32_t> fstview;
    std::vector<Entry> entries;

    explicit IndexView(std::string_view data)
    {
      uint64_t size;
      std::memcpy(&size, data.data(), sizeof(uint64_t));
      std::tie(names, book_pathes, entries, fstview.jump_table, fstview.size)
          = packme::unpack<std::tuple<decltype(names), decltype(book_pathes), decltype(entries),
            decltype(fstview.jump_table), decltype(fstview.size)> >
          (std::string_view{data.data() + sizeof(uint64_t), size});
      fstview.fst = data.data() + sizeof(uint64_t) + size;
    }

    [[nodiscard]] std::vector<std::string> search_title(const std::string& token) const
    {
      std::vector<std::string> ret;
      if (auto opt = fstview.get(token); opt.has_value())
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
      if (auto opt = fstview.get(token); opt.has_value())
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

  struct Index
  {
    std::vector<std::string> names; // store all the names
    std::vector<std::vector<size_t> > book_pathes; // store all the book pathes
    FST<uint32_t> fst; // store all the tokens
    std::vector<Entry> entries; // unique to a token

    [[nodiscard]] std::vector<char> compile() const
    {
      std::vector<char> ret;
      auto [fstdata, jump_table] = fst.compile();
      auto packed = packme::pack(std::make_tuple(names, book_pathes, entries, jump_table, fstdata.size()));
      ret.resize(packed.size() + fstdata.size() + sizeof(uint64_t));
      uint64_t size = packed.size();
      std::memcpy(ret.data(), &size, sizeof(uint64_t));
      std::memcpy(ret.data() + sizeof(uint64_t), packed.data(), packed.size());
      std::memcpy(ret.data() + sizeof(uint64_t) + packed.size(), fstdata.data(), fstdata.size());
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
        merged_entries.emplace_back(std::move(book_entries));
      }
      return Index{
        std::move(names), std::move(book_pathes),
        fst_builder.build(), std::move(merged_entries)
      };
    }
  };
}
#endif
