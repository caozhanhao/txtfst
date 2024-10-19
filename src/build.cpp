#include <iostream>
#include <filesystem>
#include <thread>
#include <mutex>
#include <atomic>

#include "txtfst/tokenizer.h"
#include "txtfst/index.h"
#include "txtfst/fst.h"

void print_usage(char** argv)
{
  std::println(std::cerr, "Usage: {} [path to index] [path to library] [options]", argv[0]);
  std::println(std::cerr, "Options:");
  std::println(std::cerr, "   -n, --no-check            Enable unchecked tokenizer", argv[0]);
  std::println(std::cerr, "   -f, --filiter [num]       Drop tokens whose length < [num]", argv[0]);
}

int main(int argc, char** argv)
{
  if (argc < 3)
  {
    print_usage(argv);
    return -1;
  }

  std::string path_to_index = argv[1];
  std::string path_to_library = argv[2];

  bool use_checked_tokenizer = true;
  int filter = -1;

  if (argc > 3)
  {
    std::vector<std::string> options;
    for (size_t i = 0; i < argc; ++i)
      options.emplace_back(argv[i]);
    for (size_t i = 3; i < options.size(); ++i)
    {
      if (options[i] == "-f" || options[i] == "-f")
      {
        if (i + 1 >= options.size())
        {
          std::println(std::cerr, "Expected a number after '{}'.", options[i]);
          return -1;
        }
        try
        {
          filter = std::stoi(options[i + 1]);
        }
        catch (...)
        {
          std::println(std::cerr, "Expected a number after '{}', found '{}'.",
                       options[i], options[i + 1]);
          return -1;
        }
        ++i;
      }
      else if (options[i] == "-n" || options[i] == "--no-check")
      {
        use_checked_tokenizer = false;
      }
      else
      {
        std::println(std::cerr, "Unknown option '{}'.", options[i]);
        print_usage(argv);
        return -1;
      }
    }
  }

  const std::filesystem::path library_path(path_to_library);

  if (!is_directory(library_path))
  {
    std::println(std::cerr, "The library path must be a valid directory.");
    return -1;
  }

  std::ofstream ofs(path_to_index, std::ios::binary);
  if (ofs.fail())
  {
    std::println(std::cerr, "Failed to write index.");
    return -1;
  }

  std::vector<std::string> pathes;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(library_path))
    if (entry.is_regular_file() && entry.path().extension() == ".txt")
      pathes.emplace_back(entry.path());

  txtfst::IndexBuilder builder;
  std::mutex output_mtx;
  std::atomic<size_t> completed(0);

  auto add_book = [&, total = pathes.size()]
  (const std::string& path)
  {
    auto [title, content, errcnt]
        = txtfst::tokenize_book(path, filter, use_checked_tokenizer);
    std::lock_guard l(output_mtx);
    if (errcnt == 1)
    {
      std::println(std::cerr,
                   "WARNING: In file '{}', {} invalid UTF-8 codepoint was ignored.",
                   path, errcnt);
    }
    else if (errcnt > 0)
    {
      std::println(std::cerr,
                   "WARNING: In file '{}', {} invalid UTF-8 codepoints were ignored.",
                   path, errcnt);
    }
    builder.add_book(path, title, content);
    ++completed;
    std::print(std::cout, "\x1b[80D\x1b[K{}/{}", completed.load(), total);
  };

  constexpr size_t NWORKER = 16;
  const size_t nwork = pathes.size() / NWORKER;
  std::array<std::thread, NWORKER> workers;
  for (size_t i = 0; i < NWORKER; ++i)
  {
    workers[i] = std::thread{
      [i, nwork, &add_book, &pathes]
      {
        for (size_t j = i * nwork; j < (i + 1) * nwork; ++j)
          add_book(pathes[j]);
      }
    };
  }

  for (size_t i = NWORKER * nwork; i < pathes.size(); ++i)
  {
    add_book(pathes[i]);
    if (auto curr = completed.fetch_add(1); curr % 500 == 0)
      std::print(std::cout, "\x1b[80D\x1b[K{}/{}", curr, pathes.size());
  }

  for (size_t i = 0; i < NWORKER; ++i)
  {
    if (workers[i].joinable())
      workers[i].join();
  }

  std::print(std::cout, "\x1b[80D\x1b[K{}/{}\n", pathes.size(), pathes.size());

  auto idx = builder.build();
  ofs << packme::pack(idx);
  ofs.close();
  return 0;
}
