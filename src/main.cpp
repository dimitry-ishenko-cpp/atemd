////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2022 Dimitry Ishenko
// Contact: dimitry (dot) ishenko (at) (gee) mail (dot) com
//
// Distributed under the GNU GPL license. See the LICENSE.md file for details.

////////////////////////////////////////////////////////////////////////////////
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>

#include "pgm/args.hpp"

constexpr auto default_bind_address = "0.0.0.0";
constexpr auto default_bind_port = 8899;

constexpr auto default_atem_port = 9910;

const auto default_bind_uri = std::string{ } + default_bind_address + ":" + std::to_string(default_bind_port);
const auto default_atem_uri = std::to_string(default_atem_port);

auto parse_uri(const std::string& uri, const std::string& default_address, int default_port)
{
    std::string address;
    int port;

    if(auto p = uri.find(':'); p != uri.npos)
    {
        address = uri.substr(0, p);
        if(address.empty()) address = default_address;

        auto tail = uri.substr(p+1);
        std::size_t done;
        try
        {
            port = std::stoi(tail, &done, 0);
            if(done != tail.size()) throw;
        }
        catch(...)
        {
            throw std::invalid_argument{"Invalid port # '" + tail + "'"};
        }
    }
    else
    {
        address = uri;
        port = default_port;
    }

    return std::make_tuple(std::move(address), port);
}

int main(int argc, char* argv[])
try
{
    namespace fs = std::filesystem;
    std::string name = fs::path{argv[0]}.filename();

    pgm::args args
    {
        { "-b", "--bind-to", "[addr][:port]", "Local end-point to bind to. Default: " + default_bind_uri },
        { "-h", "--help",                     "Print this help screen and exit." },
        { "-v", "--version",                  "Show version number and exit."    },

        { "atem-uri",                         "ATEM switcher URI as hostname[:port]. Default port: " + default_atem_uri },
    };

    std::exception_ptr ep;
    try { args.parse(argc, argv); }
    catch(...) { ep = std::current_exception(); }

    if(args["--help"])
    {
        std::cout << args.usage(name) << std::endl;
    }
    else if(args["--version"])
    {
        std::cout << name << " " << VERSION << std::endl;
    }
    else if(ep)
    {
        std::rethrow_exception(ep);
    }
    else
    {
        auto local = args["--bind-to"].value_or(default_bind_uri);
        auto [local_address, local_port] = parse_uri(local, default_bind_address, default_bind_port);

        auto remote = args["atem-uri"].value();
        auto [remote_address, remote_port] = parse_uri(remote, "", default_atem_port);

        // connect to switcher
        // create server
        // wait for connection
    }

    return 0;
}
catch(const std::exception& e)
{
    std::cerr << e.what() << std::endl;
    return 1;
}
