#include <iostream>
#include <filesystem>

#include "txtfst/tokenizer.h"
#include "txtfst/index.h"
#include "txtfst/fst.h"

int main(int argc, char** argv)
{
  if (argc != 3)
  {
    std::println(std::cerr, "Usage: {} [path to index] [path to library]", argv[0]);
    return -1;
  }

  std::string path_to_index = argv[1];
  std::string path_to_library = argv[2];

  const std::filesystem::path library_path(path_to_library);

  if (!is_directory(library_path))
  {
    std::println(std::cerr, "The library path must be a valid directory.");
    return -1;
  }

  txtfst::IndexBuilder builder;
  size_t total = std::count_if(std::filesystem::recursive_directory_iterator(library_path),
                               std::filesystem::recursive_directory_iterator{},
                               [](auto&& entry)
                               {
                                 return entry.is_regular_file() && entry.path().extension() == ".txt";
                               });
  size_t curr = 0;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(library_path))
  {
    if (entry.is_regular_file() && entry.path().extension() == ".txt")
    {
      auto [title, content, errcnt]
          = txtfst::tokenize_book(entry.path());
      if (errcnt == 1)
      {
        std::println(std::cerr,
                     "WARNING: In file '{}', {} invalid UTF-8 codepoint was ignored.",
                     entry.path().string(), errcnt);
      }
      else if (errcnt > 0)
      {
        std::println(std::cerr,
                     "WARNING: In file '{}', {} invalid UTF-8 codepoints were ignored.",
                     entry.path().string(), errcnt);
      }
      builder.add_book(entry.path(), title, content);
    }

    if (curr % 500 == 0 || curr == total - 1)
    {
      std::print(std::cout, "\x1b[80D\x1b[K{}/{}", curr, total);
      std::cout << std::flush;
    }
    ++curr;
  }
  std::print(std::cout, "\x1b[80D\x1b[K{}/{}\n", curr, total);

  auto idx = builder.build();
  std::ofstream ofs(path_to_index, std::ios::binary);
  if (ofs.fail())
  {
    std::println(std::cerr, "Failed to write index.");
    return -1;
  }
  ofs << packme::pack(idx);
  ofs.close();
  return 0;
}
