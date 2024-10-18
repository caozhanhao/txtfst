#ifndef TXTFST_TOKENIZER_H
#define TXTFST_TOKENIZER_H
#pragma once

#include <ranges>
#include <string>
#include <vector>
#include <cassert>
#include <fstream>

namespace txtfst
{
  namespace details
  {
    inline std::tuple<std::vector<std::string>, size_t> tokenize(std::string_view text)
    {
      size_t error_cnt = 0;
      auto&& rng = text
                   | std::views::chunk_by([](char, char c) { return !std::isspace(c); })
                   | std::views::transform([&error_cnt](auto&& str)
                   {
                     return
                         str
                         | std::views::chunk_by([](char, char c) { return (0b11000000 & c) == 0b10000000; })
                         | std::views::filter([&error_cnt](auto&& codepoint)
                         {
                           if ((codepoint.size() == 1 && (codepoint[0] & 0b10000000) != 0)
                               || (codepoint.size() == 2 && (codepoint[0] & 0b11100000) != 0b11000000)
                               || (codepoint.size() == 3 && (codepoint[0] & 0b11110000) != 0b11100000)
                               || (codepoint.size() == 4 && (codepoint[0] & 0b11111000) != 0b11110000)
                               || codepoint.size() > 4
                               || codepoint.size() == 0)
                           {
                             ++error_cnt;
                             return false;
                           }
                           return codepoint.size() == 1 && std::isalnum(codepoint[0]);
                         })
                         | std::views::transform([](auto&& codepoint) -> char
                         {
                           return std::tolower(codepoint[0]);
                         });
                   });
      std::vector<std::string> ret{""};
      for (auto&& i : rng)
      {
        for (auto&& j : i)
        {
          ret.back() += j;
        }
        if (!ret.back().empty())
          ret.emplace_back("");
      }
      ret.pop_back();
      return {ret, error_cnt};
    }
  }

  struct Book
  {
    std::vector<std::string> title;
    std::vector<std::string> content;
    size_t error_cnt{0};
  };

  inline Book tokenize_book(const std::string& path)
  {
    std::ifstream ifs(path);
    assert(!ifs.fail());
    std::string buf;
    buf.resize(ifs.seekg(0, std::ios::end).tellg());
    ifs.seekg(0, std::ios::beg).read(buf.data(), static_cast<std::streamsize>(buf.size()));
    ifs.close();
    std::string_view text{buf};

    Book book;

    auto a = buf.find('\n');
    assert(a != std::string::npos);
    size_t content_ecnt = 0;
    std::tie(book.title, book.error_cnt) = details::tokenize(text.substr(0, a));
    std::tie(book.content, content_ecnt) = details::tokenize(text.substr(a));
    book.error_cnt += content_ecnt;

    return book;
  }
}
#endif
