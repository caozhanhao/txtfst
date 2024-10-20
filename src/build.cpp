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
  std::println(std::cerr, "   -j, --jobs [num]          Start n jobs, defaults to be 1", argv[0]);
  std::println(std::cerr, "   -c, --chunk [num]         Set chunk size, defaults to be 5000", argv[0]);
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
  size_t build_worker = 0;
  size_t chunk_size = 5000;

  if (argc > 3)
  {
    std::vector<std::string> options;
    for (size_t i = 3; i < argc; ++i)
      options.emplace_back(argv[i]);
    for (size_t i = 0; i < options.size(); ++i)
    {
      if (options[i] == "-f" || options[i] == "--filiter")
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
      else if (options[i] == "-j" || options[i] == "--jobs")
      {
        if (i + 1 >= options.size())
        {
          std::println(std::cerr, "Expected a number after '{}'.", options[i]);
          return -1;
        }
        try
        {
          if(int a = std::stoi(options[i + 1]) - 1; a < 0)
          {
            std::println(std::cerr, "Expected a non-zero positive number after '{}', found '{}'.",
             options[i], options[i + 1]);
            return -1;
          }
          else
            build_worker = a;
        }
        catch (...)
        {
          std::println(std::cerr, "Expected a number after '{}', found '{}'.",
                       options[i], options[i + 1]);
          return -1;
        }
        ++i;
      }
      else if (options[i] == "-c" || options[i] == "--chunk")
      {
        if (i + 1 >= options.size())
        {
          std::println(std::cerr, "Expected a number after '{}'.", options[i]);
          return -1;
        }
        try
        {
          chunk_size = std::stoul(options[i + 1]);
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

  size_t chunk_perworker = 0;
  if(build_worker != 0)
  {
    chunk_perworker = pathes.size() / chunk_size / build_worker;
    while (chunk_perworker == 0 && build_worker > 1)
    {
      --build_worker;
      chunk_perworker = pathes.size() / chunk_size / build_worker;
    }
  }

  std::vector<std::thread> workers;
  workers.resize(build_worker);
  std::vector<txtfst::IndexBuilder> builders;
  builders.resize(build_worker + 1);
  std::vector<size_t> curr_chunk;
  curr_chunk.resize(build_worker + 1);
  std::mutex output_mtx;
  std::atomic<size_t> completed(0);

  auto add_book = [&, total = pathes.size()]
  (size_t worker_id, const std::string& path, txtfst::IndexBuilder& builder)
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
    if (++curr_chunk[worker_id] == chunk_size)
    {
      auto idx = builder.build().compile();
      uint64_t idx_size = idx.size();
      output_mtx.lock();
      ofs.write(reinterpret_cast<char*>(&idx_size), sizeof(uint64_t));
      ofs.write(idx.data(), static_cast<std::streamsize>(idx.size()));
      output_mtx.unlock();
      builder = txtfst::IndexBuilder{};
      curr_chunk[worker_id] = 0;
    }
    output_mtx.lock();
    std::print(std::cout, "\x1b[80D\x1b[K{}/{}", completed.load(), total);
    output_mtx.unlock();
  };

  std::println(std::cout, "Start building index for '{}'.", path_to_library);

  auto start = std::chrono::system_clock::now();

  if (chunk_perworker != 0)
  {
    for (size_t i = 0; i < build_worker; ++i)
    {
      workers[i] = std::thread{
        [i, chunk_perworker, chunk_size, &add_book, &pathes, &builders]
        {
          for (size_t j = i * chunk_perworker * chunk_size; j < (i + 1) * chunk_perworker * chunk_size; ++j)
            add_book(i, pathes[j], builders[i]);
        }
      };
    }
  }

  if(build_worker * chunk_perworker * chunk_size < pathes.size())
  {
    for (size_t i = build_worker * chunk_perworker * chunk_size; i < pathes.size(); ++i)
      add_book(build_worker, pathes[i], builders[build_worker]);
    auto idx = builders[build_worker].build().compile();
    uint64_t idx_size = idx.size();
    output_mtx.lock();
    ofs.write(reinterpret_cast<char*>(&idx_size), sizeof(uint64_t));
    ofs.write(idx.data(), static_cast<std::streamsize>(idx.size()));
    output_mtx.unlock();
  }

  if(chunk_perworker != 0)
  {
    for (size_t i = 0; i < build_worker; ++i)
    {
      if (workers[i].joinable())
        workers[i].join();
    }
  }

  std::print(std::cout, "\x1b[80D\x1b[K{}/{}\n", pathes.size(), pathes.size());

  auto end = std::chrono::system_clock::now();
  std::println(std::cout, "Successfully built index at '{}', time: {} s", path_to_index,
                static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()) /
                1000.0);

  ofs.close();
  return 0;
}
