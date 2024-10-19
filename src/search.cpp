#include <iostream>
#include <fstream>

#include "txtfst/index.h"

void print_usage(char** argv)
{
    std::println(std::cerr,
                 "Usage: {} [path to index] [options] [tokens]", argv[0]);
    std::println(std::cerr, "Options:");
    std::println(std::cerr, "   -t, --title       Search in title");
    std::println(std::cerr, "   -c, --content     Search in content");
}

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        print_usage(argv);
        return -1;
    }

    std::string path_to_index = argv[1];
    std::string option = argv[2];
    std::vector<std::string> tokens{""};
    for (int i = 3; i < argc; ++i)
    {
        for (size_t j = 0; argv[i][j] != '\0'; ++j)
            tokens.back() += static_cast<char>(std::tolower(argv[i][j]));
        tokens.emplace_back("");
    }
    tokens.pop_back();

    bool search_title = false;
    if (option == "-t" || option == "--title")
    {
        search_title = true;
    }
    else if (option == "-c" || option == "--content")
    {
        search_title = false;
    }
    else
    {
        std::println(std::cerr, "Unknown option '{}'.", option);
        print_usage(argv);
        return -1;
    }

    std::ifstream ifs(path_to_index, std::ios::binary);
    if (ifs.fail())
    {
        std::println(std::cerr, "Failed to open index.");
        return -1;
    }
    std::string buf;
    buf.resize(ifs.seekg(0, std::ios::end).tellg());
    ifs.seekg(0, std::ios::beg).read(buf.data(), static_cast<std::streamsize>(buf.size()));
    ifs.close();
    auto index = packme::unpack<txtfst::Index>(buf);

    for (auto&& token : tokens)
    {
        std::vector<std::string> result;
        if (search_title)
            result = index.search_title(token);
        else
            result = index.search_content(token);
        if (!result.empty())
        {
            std::println(std::cout, "{}:", token);
            for (auto&& r : result)
            {
                std::println(std::cout, "{}", r);
            }
        }
        else
        {
            std::println(std::cout, "{} not found.", token);
        }
    }
    return 0;
}
