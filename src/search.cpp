#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <chrono>

#include "txtfst/index.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

void print_usage(char** argv)
{
  std::println(std::cerr,
               "Usage: {} [path to index] [options] [tokens]", argv[0]);
  std::println(std::cerr, "Options:");
  std::println(std::cerr, "   -t, --title            Search in title");
  std::println(std::cerr, "   -c, --content          Search in content");
  std::println(std::cerr, "   -j, --jobs [num]       Start n jobs, defaults to be 1", argv[0]);
}

int main(int argc, char** argv)
{
  if (argc < 4)
  {
    print_usage(argv);
    return -1;
  }

  std::string path_to_index = argv[1];

  bool search_title = false;
  size_t search_worker = 0;
  std::vector<std::string> options;
  size_t argpos = 2;
  for (; argpos < argc; ++argpos)
  {
    if(argv[argpos][0] != '-' && argpos + 1 < argc && argv[argpos + 1][0] != '-') break;
    options.emplace_back(argv[argpos]);
  }
  for (size_t i = 0; i < options.size(); ++i)
  {
    if (options[i] == "-c" || options[i] == "--content")
    {
      search_title = false;
    }
    else if (options[i] == "-t" || options[i] == "--title")
    {
      search_title = true;
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
        }
        else
          search_worker = a;
      }
      catch (...)
      {
        std::println(std::cerr, "Expected a number after '{}', found '{}'.",
                     options[i], options[i + 1]);
        return -1;
      }
      ++i;
    }
    else
    {
      std::println(std::cerr, "Unknown option '{}'.", options[i]);
      print_usage(argv);
      return -1;
    }
  }

  std::vector<std::string> tokens{""};
  for (int i = argpos; i < argc; ++i)
  {
    for (size_t j = 0; argv[i][j] != '\0'; ++j)
      tokens.back() += static_cast<char>(std::tolower(argv[i][j]));
    tokens.emplace_back("");
  }
  tokens.pop_back();

  std::println(std::cout, "Loading index from '{}'.", path_to_index);

  auto start = std::chrono::system_clock::now();

  int fd = open(path_to_index.c_str(), O_RDONLY);
  struct stat statbuf{};
  if (fd < 0 || stat(path_to_index.c_str(), &statbuf) != 0)
  {
    std::println(std::cerr, "Failed to open index.");
    exit(-1);
  }
  auto ptr = static_cast<char*>(mmap(nullptr, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0));
  std::string_view indexdata{ptr, static_cast<size_t>(statbuf.st_size)};

  std::vector<std::string_view> packed;
  for (size_t i = 0; i < indexdata.size();)
  {
    uint64_t size;
    std::memcpy(&size, indexdata.data() + i, sizeof(uint64_t));
    packed.emplace_back(indexdata.substr(i + sizeof(uint64_t), size));
    i += size + sizeof(uint64_t);
  }

  std::mutex add_mtx;
  size_t work_perworker = 0;
  if(search_worker != 0)
  {
    work_perworker = packed.size() / search_worker;
  }
  std::vector<std::thread> workers;
  workers.resize(search_worker);
  std::vector<std::vector<std::string>> result;
  result.resize(tokens.size());

  auto load_and_search = [search_title, &result, &add_mtx, &tokens](std::string_view raw_index)
  {
    txtfst::IndexView index(raw_index);
    for (size_t i = 0; i < tokens.size(); ++i)
    {
      if (search_title)
      {
        auto a = index.search_title(tokens[i]);
        add_mtx.lock();
        result[i].insert(result[i].end(), std::make_move_iterator(a.begin()),
                      std::make_move_iterator(a.end()));
        add_mtx.unlock();
      }
      else
      {
        auto a = index.search_content(tokens[i]);
        add_mtx.lock();
        result[i].insert(result[i].end(), std::make_move_iterator(a.begin()),
                      std::make_move_iterator(a.end()));
        add_mtx.unlock();
      }
    }
  };

  if(work_perworker != 0)
  {
    for (size_t i = 0; i < search_worker; ++i)
    {
      workers[i] = std::thread{
        [work_perworker, i, &packed, &load_and_search]
        {
          for (size_t j = i * work_perworker; j < (i + 1) * work_perworker; ++j)
            load_and_search(packed[j]);
        }
      };
    }
  }

  for (size_t i = search_worker * work_perworker; i < packed.size(); ++i)
    load_and_search(packed[i]);

  if(work_perworker != 0)
  {
    for (size_t i = 0; i < search_worker; ++i)
    {
      if (workers[i].joinable())
        workers[i].join();
    }
  }

  for (size_t i = 0; i < tokens.size(); ++i)
  {
    if (!result[i].empty())
    {
      std::println(std::cout, "{}:", tokens[i]);
      for (auto&& r : result[i])
        std::println(std::cout, "{}", r);
    }
    else
      std::println(std::cout, "{} not found.", tokens[i]);
  }

  auto end = std::chrono::system_clock::now();
  std::println(std::cout, "Successfully searched tokens. time: {} s.",
               static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>
                 (end - start).count()) / 1000.0);

  munmap(ptr, statbuf.st_size);
  return 0;
}
