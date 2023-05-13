////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2022-2023 Dimitry Ishenko
// Contact: dimitry (dot) ishenko (at) (gee) mail (dot) com
//
// Distributed under the GNU GPL license. See the LICENSE.md file for details.

////////////////////////////////////////////////////////////////////////////////
#include "atemd.hpp"
#include "pgm/args.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>

////////////////////////////////////////////////////////////////////////////////
const std::string default_bind_address = "0.0.0.0";
const std::string default_bind_port = "8899";

const std::string default_atem_port = "9910";

const std::string default_bind_uri = default_bind_address + ":" + default_bind_port;

////////////////////////////////////////////////////////////////////////////////
auto parse_uri(const std::string& uri, const std::string& default_address, const std::string& default_port)
{
    std::string address, port;

    if(auto p = uri.find(':'); p != uri.npos)
    {
        address = uri.substr(0, p);
        port = uri.substr(p+1);

        if(address.empty()) address = default_address;
    }
    else
    {
        address = uri;
        port = default_port;
    }

    return std::make_tuple(std::move(address), std::move(port));
}

////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
try
{
    namespace fs = std::filesystem;
    auto name = fs::path{argv[0]}.filename().string();

    pgm::args args
    {
        { "-b", "--bind-to", "[addr][:port]", "Local end-point to bind to. Default: " + default_bind_uri },
        { "-h", "--help",                     "Print this help screen and exit." },
        { "-v", "--version",                  "Show version number and exit."    },

        { "atem-uri",                         "ATEM URI in the form hostname[:port]. Default port: " + default_atem_port },
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
        auto bind = args["--bind-to"].value_or(default_bind_uri);
        auto [bind_address, bind_port] = parse_uri(bind, default_bind_address, default_bind_port);

        auto atem = args["atem-uri"].value();
        auto [atem_address, atem_port] = parse_uri(atem, "", default_atem_port);

        atemd atemd{ bind_address, bind_port, atem_address, atem_port };
        atemd.run();
    }

    return 0;
}
catch(const std::exception& e)
{
    std::cerr << e.what() << std::endl;
    return 1;
}