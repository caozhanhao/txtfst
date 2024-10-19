#include <iostream>

#include "txtfst/tokenizer.h"

void print_usage(char** argv)
{
  std::println(std::cerr, "Usage: {} [path to file] [options]", argv[0]);
  std::println(std::cerr, "Options:");
  std::println(std::cerr, "   -n, --no-check            Enable unchecked tokenizer", argv[0]);
  std::println(std::cerr, "   -f, --filiter [num]       Drop tokens whose length < [num]", argv[0]);
}

int main(int argc, char** argv)
{
  if (argc < 2)
  {
    print_usage(argv);
    return -1;
  }

  bool use_checked_tokenizer = true;
  int filter = -1;

  std::string path = argv[1];

  if (argc > 2)
  {
    std::vector<std::string> options;
    for (size_t i = 0; i < argc; ++i)
      options.emplace_back(argv[i]);

    for (size_t i = 2; i < options.size(); ++i)
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
  auto [title, content, errcnt]
      = txtfst::tokenize_book(path, filter, use_checked_tokenizer);

  std::println(std::cout, "'{}': ", path);
  std::print(std::cout, "    Title: ");
  for (auto&& token : title)
    std::print(std::cout, "'{}' ", token);
  std::print(std::cout, "\n    Content: ");
  for (size_t i = 0; i < content.size(); ++i)
  {
    std::print(std::cout, "'{}' ", content[i]);
    if (i != 0 && i % 8 == 0 && i + 1 != content.size())
      std::print(std::cout, "\n    ");
  }
  std::println(std::cout, "\n    Error: {}", errcnt);

  return 0;
}
