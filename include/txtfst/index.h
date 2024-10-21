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
  struct BookEntry
  {
    size_t idx{0};
    size_t title_freq{0};
    size_t content_freq{0};
  };

  struct Entry
  {
    std::vector<BookEntry> books;
  };

  struct CompiledEntriesView
  {
    const uint64_t* jump_table{};
    size_t jump_table_size{};
    const BookEntry* books{nullptr};
    size_t size{};
  };

  struct CompiledNamesView
  {
    const uint64_t* jump_table{};
    size_t jump_table_size{};
    const char* names{nullptr};
  };

  struct CompiledPathsView
  {
    const uint64_t* jump_table{};
    size_t jump_table_size{};
    const uint32_t* paths{nullptr};
    size_t size{};
  };

  struct IndexView
  {
    CompiledFSTView<uint32_t> fst_view;
    CompiledEntriesView entries_view;
    CompiledPathsView paths_view;
    CompiledNamesView names_view;

    explicit IndexView(std::string_view data)
    {
      uint64_t size;
      std::memcpy(&size, data.data(), sizeof(uint64_t));
      auto [ntpos, ntlen, ptpos, ptlen, etpos, etlen, ftpos, ftlen]
          = packme::unpack<std::tuple<size_t, size_t, size_t, size_t, size_t, size_t, size_t, size_t> >
          (std::string_view{data.data() + sizeof(uint64_t), size});
      size_t offset = size + sizeof(uint64_t);

      names_view.jump_table = reinterpret_cast<const uint64_t*>(data.data() + offset + ntpos);
      names_view.jump_table_size = ntlen;
      names_view.names = const_cast<char*>(data.data() + offset + ntpos + ntlen * sizeof(uint64_t));

      paths_view.jump_table = reinterpret_cast<const uint64_t*>(data.data() + offset + ptpos);
      paths_view.jump_table_size = ptlen;
      paths_view.paths = reinterpret_cast<const uint32_t*>(data.data() + offset + ptpos + ptlen * sizeof(uint64_t));
      paths_view.size = etpos - ptpos - ptlen * sizeof(uint64_t);

      entries_view.jump_table = reinterpret_cast<const uint64_t*>(data.data() + offset + etpos);
      entries_view.jump_table_size = etlen;
      entries_view.books = reinterpret_cast<const BookEntry*>(data.data() + offset + etpos + etlen * sizeof(uint64_t));
      entries_view.size = ftpos - etpos - etlen * sizeof(uint64_t);

      fst_view.jump_table = reinterpret_cast<const uint64_t*>(data.data() + offset + ftpos);
      fst_view.jump_table_size = ftlen;
      fst_view.fst = data.data() + offset + ftpos + ftlen * sizeof(uint64_t);
      fst_view.fst_size = data.size() - offset - ftpos - ftlen * sizeof(uint64_t);
    }

    [[nodiscard]] std::vector<std::string> search_title(const std::string& token) const
    {
      return search(token, [](auto&& r) { return r.title_freq; });
    }

    [[nodiscard]] std::vector<std::string> search_content(const std::string& token) const
    {
      return search(token, [](auto&& r) { return r.content_freq; });
    }

  private:
    template<typename Proj>
    [[nodiscard]] std::vector<std::string> search(const std::string& token, Proj&& proj) const
    {
      std::vector<std::string> ret;
      if (auto opt = fst_view.get(token); opt.has_value())
      {
        std::vector<BookEntry> entries;

        auto ecurr = entries_view.books + entries_view.jump_table[*opt] / sizeof(BookEntry);
        size_t elen = 0;
        if (*opt != entries_view.jump_table_size - 1)
          elen = entries_view.books + entries_view.jump_table[*opt + 1] / sizeof(BookEntry) - ecurr;
        else
          elen = entries_view.books + entries_view.size / sizeof(BookEntry) - ecurr;
        for (size_t i = 0; i < elen; ++i)
          entries.emplace_back(*(ecurr + i));
        // We use different chunks to store fst, so the sort is meaningless.
        // std::ranges::sort(entries, std::greater{}, std::forward<Proj>(proj));
        for (auto& entry : entries)
        {
          // Since we didn't sort, we need to `continue` rather than `break`.
          if (proj(entry) == 0)
            continue;
          std::vector<size_t> paths;
          auto pcurr = paths_view.paths + paths_view.jump_table[entry.idx] / sizeof(uint32_t);
          size_t plen = 0;
          if (entry.idx != paths_view.jump_table_size - 1)
            plen = paths_view.paths + paths_view.jump_table[entry.idx + 1] / sizeof(uint32_t) - pcurr;
          else
            plen = paths_view.paths + paths_view.size / sizeof(uint32_t) - pcurr;
          for (size_t i = 0; i < plen; ++i)
            paths.emplace_back(*(pcurr + i));

          std::string path;

          for(auto&& name_idx : paths)
          {
            auto ncurr = names_view.names + names_view.jump_table[name_idx];
            for(;*ncurr != '\0'; ++ncurr)
              path += *ncurr;
            path += "/";
          }
          path.pop_back();
          ret.emplace_back(path);
        }
      }
      return ret;
    }
  };

  struct Index
  {
    FST<uint32_t> fst; // store all the tokens
    std::vector<Entry> entries; // unique to a token
    std::vector<std::vector<uint32_t> > book_paths; // store all the book paths
    std::vector<std::string> names; // store all the names

    [[nodiscard]] std::vector<char> compile() const
    {
      std::vector<char> ret;

      std::vector<uint64_t> names_table;
      names_table.resize(names.size());
      ret.resize(names_table.size() * sizeof(uint64_t));
      size_t offset = ret.size();
      for (size_t i = 0; i < names.size(); ++i)
      {
        names_table[i] = ret.size() - offset;
        ret.insert(ret.end(), names[i].cbegin(), names[i].cend());
        ret.insert(ret.end(), '\0');
      }
      std::memmove(ret.data(), names_table.data(), names_table.size() * sizeof(uint64_t));

      size_t paths_table_pos = ret.size();
      std::vector<uint64_t> paths_table;
      paths_table.resize(book_paths.size());
      ret.resize(ret.size() + paths_table.size() * sizeof(uint64_t));

      offset = ret.size();
      size_t path_offset = 0;
      for (size_t i = 0; i < book_paths.size(); ++i)
      {
        paths_table[i] = ret.size() - offset;
        ret.resize(ret.size() + book_paths[i].size() * sizeof(uint32_t));
        std::memmove(ret.data() + offset + path_offset, book_paths[i].data(),
                     sizeof(uint32_t) * book_paths[i].size());
        path_offset += sizeof(uint32_t) * book_paths[i].size();
      }
      std::memmove(ret.data() + paths_table_pos, paths_table.data(), paths_table.size() * sizeof(uint64_t));

      size_t entries_table_pos = ret.size();
      std::vector<uint64_t> entries_table;
      entries_table.resize(entries.size());
      ret.resize(ret.size() + entries_table.size() * sizeof(uint64_t));
      offset = ret.size();
      size_t entry_offset = 0;
      for (size_t i = 0; i < entries.size(); ++i)
      {
        entries_table[i] = ret.size() - offset;
        ret.resize(ret.size() + entries[i].books.size() * sizeof(BookEntry));
        std::memmove(ret.data() + offset + entry_offset, entries[i].books.data(),
                     entries[i].books.size() * sizeof(BookEntry));
        entry_offset += entries[i].books.size() * sizeof(BookEntry);
      }
      std::memmove(ret.data() + entries_table_pos, entries_table.data(), entries_table.size() * sizeof(uint64_t));

      size_t fst_table_pos = ret.size();
      std::vector<uint64_t> fst_table;
      fst_table.resize(fst.states.size());
      ret.resize(ret.size() + fst_table.size() * sizeof(uint64_t));

      offset = ret.size();
      size_t fst_offset = 0;
      for (const auto& state : fst.states)
      {
        size_t expected = sizeof(state.id) + sizeof(state.final) + state.trans.size() * sizeof(State<uint32_t>::Arc);
        if (ret.size() - offset - fst_offset < expected)
          ret.resize(ret.size() + expected * 2);

        fst_table[state.id] = fst_offset;
        std::memmove(ret.data() + offset + fst_offset, &state.id, sizeof(state.id));
        fst_offset += sizeof(state.id);
        std::memmove(ret.data() + offset + fst_offset, &state.final, sizeof(state.final));
        fst_offset += sizeof(state.final);
        for (auto&& arc : state.trans)
        {
          std::memmove(ret.data() + offset + fst_offset, &arc, sizeof(arc));
          fst_offset += sizeof(arc);
        }
      }
      std::memmove(ret.data() + fst_table_pos, fst_table.data(), fst_table.size() * sizeof(uint64_t));

      auto packed = packme::pack(std::make_tuple(0, names_table.size(), paths_table_pos, paths_table.size(),
                                                 entries_table_pos, entries_table.size(), fst_table_pos,
                                                 fst_table.size()));
      size_t packed_size = packed.size();


      std::vector<char> final;
      final.resize(packed_size + ret.size() + sizeof(uint64_t));
      std::memmove(final.data(), &packed_size, sizeof(uint64_t));
      std::memmove(final.data() + sizeof(uint64_t), packed.data(), packed_size);
      std::memmove(final.data() + sizeof(uint64_t) + packed_size, ret.data(), ret.size());

      return final;
    }
  };

  class IndexBuilder
  {
    std::vector<std::string> names;
    std::vector<std::vector<uint32_t> > book_paths;
    std::vector<Entry> merged_entries;
    std::map<std::string, std::map<size_t, BookEntry> > unmerged_tokens;
    FSTBuilder<uint32_t> fst_builder;

  public:
    IndexBuilder& add_book(const std::string& path,
                           const std::vector<std::string>& title,
                           const std::vector<std::string>& content)
    {
      book_paths.emplace_back();
      for (auto&& name : path | std::views::split('/'))
      {
        auto sv = std::string_view{name};
        auto it = std::ranges::find(names, sv);
        if (it == names.end())
        {
          names.emplace_back(sv);
          book_paths.back().emplace_back(names.size() - 1);
        }
        else
          book_paths.back().emplace_back(it - names.cbegin());
      }

      auto curr_book = book_paths.size() - 1;
      for (auto&& token : title)
      {
        auto& curr_entry = unmerged_tokens[token];
        if (auto it = curr_entry.find(curr_book); it == curr_entry.end())
          curr_entry[curr_book] = BookEntry{curr_book, 1, 0};
        else
          ++it->second.title_freq;
      }

      for (auto&& token : content)
      {
        auto& curr_entry = unmerged_tokens[token];
        if (auto it = curr_entry.find(curr_book); it == curr_entry.end())
          curr_entry[curr_book] = BookEntry{curr_book, 0, 1};
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
        std::vector<BookEntry> book_entries;
        for (auto&& t : r.second)
          book_entries.emplace_back(t.second);
        merged_entries.emplace_back(std::move(book_entries));
      }
      return Index{fst_builder.build(), std::move(merged_entries), std::move(book_paths), std::move(names)};
    }
  };
}
#endif
