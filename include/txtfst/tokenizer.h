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
    inline std::tuple<std::vector<std::string>, size_t> tokenize(std::string_view text, int filiter)
    {
      size_t error_cnt = 0;
      auto&& valid_utf8 = text
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
                            return true;
                          });

      std::vector<std::string> ret{""};
      for (auto&& codepoint : valid_utf8)
      {
        if (codepoint.size() == 1 && std::isalnum(codepoint[0]))
          ret.back() += static_cast<char>(std::tolower(codepoint[0]));
        else if (!ret.back().empty())
        {
          if (filiter != -1 && ret.back().size() < filiter)
            ret.back().clear();
          else
            ret.emplace_back("");
        }
      }
      ret.pop_back();
      return {ret, error_cnt};
    }

    inline std::vector<std::string> unchecked_tokenize(std::string_view text, int filiter)
    {
      std::vector<std::string> ret{""};
      for (auto&& r : text)
      {
        if (std::isalnum(r))
          ret.back() += static_cast<char>(std::tolower(r));
        else if (!ret.back().empty())
        {
          if (filiter != -1 && ret.back().size() < filiter)
            ret.back().clear();
          else
            ret.emplace_back("");
        }
      }
      if (ret.back().empty())
        ret.pop_back();
      return ret;
    }
  }

  struct Book
  {
    std::vector<std::string> title;
    std::vector<std::string> content;
    size_t error_cnt{0};
  };

  inline Book tokenize_book(const std::string& path, int filiter, bool check)
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
    if (check)
    {
      std::tie(book.title, book.error_cnt) = details::tokenize(text.substr(0, a), filiter);
      std::tie(book.content, content_ecnt) = details::tokenize(text.substr(a), filiter);
    }
    else
    {
      book.title = details::unchecked_tokenize(text.substr(0, a), filiter);
      book.content = details::unchecked_tokenize(text.substr(a), filiter);
    }
    book.error_cnt += content_ecnt;

    return book;
  }
}
#endif
