#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>

#include "txtfst/index.h"

constexpr size_t SEARCH_WORKER = 16;

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

    std::string_view indexdata{buf};

    std::vector<std::string_view> packed;
    for (size_t i = 0; i < indexdata.size();)
    {
        uint64_t size;
        std::memcpy(&size, indexdata.data() + i, sizeof(uint64_t));
        packed.emplace_back(indexdata.substr(i + sizeof(uint64_t), size));
        i += size + sizeof(uint64_t);
    }

    std::vector<txtfst::Index> indexes;
    std::mutex add_mtx;
    const size_t nwork = packed.size() / SEARCH_WORKER;
    std::array<std::thread, SEARCH_WORKER> workers;

    for (size_t i = 0; i < SEARCH_WORKER; ++i)
    {
        workers[i] = std::thread{
            [nwork, i, &packed, &add_mtx, &indexes]
            {
                for (size_t j = i * nwork; j < (i + 1) * nwork; ++j)
                {
                    auto unpacked = packme::unpack<txtfst::Index>(packed[j]);
                    add_mtx.lock();
                    indexes.emplace_back(unpacked);
                    add_mtx.unlock();
                }
            }
        };
    }

    for (size_t i = SEARCH_WORKER * nwork; i < packed.size(); ++i)
    {
        auto unpacked = packme::unpack<txtfst::Index>(packed[i]);
        add_mtx.lock();
        indexes.emplace_back(unpacked);
        add_mtx.unlock();
    }

    for (size_t i = 0; i < SEARCH_WORKER; ++i)
    {
        if (workers[i].joinable())
            workers[i].join();
    }

    for (auto&& token : tokens)
    {
        std::vector<std::string> result;
        for (auto&& index : indexes)
        {
            auto do_search = [&result, &search_title, &token, &add_mtx](const txtfst::Index& index)
            {
                if (search_title)
                {
                    auto a = index.search_title(token);
                    add_mtx.lock();
                    result.insert(result.end(), std::make_move_iterator(a.begin()),
                                  std::make_move_iterator(a.end()));
                    add_mtx.unlock();
                }
                else
                {
                    auto a = index.search_content(token);
                    add_mtx.lock();
                    result.insert(result.end(), std::make_move_iterator(a.begin()),
                                  std::make_move_iterator(a.end()));
                    add_mtx.unlock();
                }
            };
            for (size_t i = 0; i < SEARCH_WORKER; ++i)
            {
                workers[i] = std::thread{
                    [nwork, i, &indexes, &do_search]
                    {
                        for (size_t j = i * nwork; j < (i + 1) * nwork; ++j)
                            do_search(indexes[j]);
                    }
                };
            }

            for (size_t i = SEARCH_WORKER * nwork; i < packed.size(); ++i)
                do_search(indexes[i]);

            for (size_t i = 0; i < SEARCH_WORKER; ++i)
            {
                if (workers[i].joinable())
                    workers[i].join();
            }
        }

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
