#include <iostream>
#include <fstream>

#include "txtfst/index.h"

int main(int argc, char **argv)
{
    if(argc != 4)
    {
        std::println(std::cerr,
            "Usage: {} [path to index] [options] [token]", argv[0]);
        std::println(std::cerr, "Options:");
        std::println(std::cerr, "   -t, --title       Search in title");
        std::println(std::cerr, "   -c, --content     Search in content");
        return -1;
    }

    std::string path_to_index = argv[1];
    std::string option = argv[2];
    std::string token = argv[3];
    bool search_title = false;
    if(option == "-t" || option == "--title")
    {
        search_title = true;
    }
    else if(option == "-c" || option == "--content")
    {
        search_title = false;
    }
    else
    {
        std::println(std::cerr, "Unknown option '{}'.", option);
        return -1;
    }

    std::ifstream ifs(path_to_index, std::ios::binary);
    if(ifs.fail())
    {
        std::println(std::cerr, "Failed to open index.");
        return -1;
    }
    std::string buf;
    buf.resize(ifs.seekg(0, std::ios::end).tellg());
    ifs.seekg(0, std::ios::beg).read(buf.data(), static_cast<std::streamsize>(buf.size()));
    ifs.close();
    auto index = packme::unpack<txtfst::Index>(buf);
    std::vector<std::string> result;

    std::string search_token;
    for(auto&& r : token)
        search_token += static_cast<char>(std::tolower(r));
    if(search_title)
        result = index.search_title(search_token);
    else
        result = index.search_content(search_token);

    for (auto&& r : result)
    {
        std::println(std::cout, "{}", r);
    }
    return 0;
}
