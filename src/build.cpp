#include <iostream>
#include <filesystem>
#include <thread>
#include <mutex>
#include <atomic>

#include "txtfst/tokenizer.h"
#include "txtfst/index.h"
#include "txtfst/fst.h"

constexpr size_t BUILD_WORKER = 16;
constexpr size_t BUILDER_CHUNK_SIZE = 20000;

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

  const size_t nwork = pathes.size() / BUILD_WORKER;
  std::array<std::thread, BUILD_WORKER> workers;
  std::array<txtfst::IndexBuilder, BUILD_WORKER + 1> builders;
  std::mutex output_mtx;
  std::atomic<size_t> completed(0);
  std::atomic<size_t> curr_chunk(0);

  auto add_book = [&, total = pathes.size()]
  (const std::string& path, txtfst::IndexBuilder& builder)
  {
    auto [title, content, errcnt]
        = txtfst::tokenize_book(path, filter, use_checked_tokenizer);
    if (errcnt == 1)
    {
      std::lock_guard l(output_mtx);
      std::println(std::cerr,
                   "WARNING: In file '{}', {} invalid UTF-8 codepoint was ignored.",
                   path, errcnt);
    }
    else if (errcnt > 0)
    {
      std::lock_guard l(output_mtx);
      std::println(std::cerr,
                   "WARNING: In file '{}', {} invalid UTF-8 codepoints were ignored.",
                   path, errcnt);
    }
    builder.add_book(path, title, content);
    ++completed;
    if (++curr_chunk > BUILDER_CHUNK_SIZE)
    {
      auto idx = packme::pack(builder.build());
      uint64_t idx_size = idx.size();
      output_mtx.lock();
      ofs.write(reinterpret_cast<char*>(&idx_size), sizeof(uint64_t));
      ofs.write(idx.data(), static_cast<std::streamsize>(idx.size()));
      output_mtx.unlock();
      builder = txtfst::IndexBuilder{};
      curr_chunk = 0;
    }
    std::print(std::cout, "\x1b[80D\x1b[K{}/{}", completed.load(), total);
  };

  for (size_t i = 0; i < BUILD_WORKER; ++i)
  {
    workers[i] = std::thread{
      [i, nwork, &add_book, &pathes, &builders]
      {
        for (size_t j = i * nwork; j < (i + 1) * nwork; ++j)
          add_book(pathes[j], builders[i]);
      }
    };
  }

  for (size_t i = BUILD_WORKER * nwork; i < pathes.size(); ++i)
  {
    add_book(pathes[i], builders[BUILD_WORKER]);
    if (auto curr = completed.fetch_add(1); curr % 500 == 0)
      std::print(std::cout, "\x1b[80D\x1b[K{}/{}", curr, pathes.size());
  }

  for (size_t i = 0; i < BUILD_WORKER; ++i)
  {
    if (workers[i].joinable())
      workers[i].join();
  }

  for (size_t i = 0; i < BUILD_WORKER + 1; ++i)
  {
    auto idx = packme::pack(builders[i].build());
    if (!idx.empty())
    {
      uint64_t idx_size = idx.size();
      ofs.write(reinterpret_cast<char*>(&idx_size), sizeof(uint64_t));
      ofs.write(idx.data(), static_cast<std::streamsize>(idx.size()));
    }
  }

  std::print(std::cout, "\x1b[80D\x1b[K{}/{}\n", pathes.size(), pathes.size());

  ofs.close();
  return 0;
}
