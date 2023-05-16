////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2022-2023 Dimitry Ishenko
// Contact: dimitry (dot) ishenko (at) (gee) mail (dot) com
//
// Distributed under the GNU GPL license. See the LICENSE.md file for details.

////////////////////////////////////////////////////////////////////////////////
#include "atemd.hpp"
#include "pgm/args.hpp"
#include "win/service.hpp"

#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>

namespace fs = std::filesystem;

using namespace std::chrono_literals;

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
void run_program(std::string_view bind_address, std::string_view bind_port, std::string_view atem_address, std::string_view atem_port);
void run_service(std::string name, std::string_view bind_address, std::string_view bind_port, std::string_view atem_address, std::string_view atem_port);

////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
try
{
    auto path = fs::path{argv[0]};
    auto name = path.filename().string();

    pgm::args args
    {
        { "-b", "--bind-to", "[addr][:port]", "Local end-point to bind to. Default: " + default_bind_uri },
        { "-f", "--log-to-file",              "Log to file instead of console."  },
        { "-s", "--service",                  "Run as Windows service."          },

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
        std::ofstream file;
        if(args["--log-to-file"])
        {
            file.open(path.replace_extension(".log"));
            if(file) std::cout.rdbuf(file.rdbuf()); // redirect std::cout to file
        }

        auto bind = args["--bind-to"].value_or(default_bind_uri);
        auto [bind_address, bind_port] = parse_uri(bind, default_bind_address, default_bind_port);

        auto atem = args["atem-uri"].value();
        auto [atem_address, atem_port] = parse_uri(atem, "", default_atem_port);

        if(args["--service"])
            run_service(name, bind_address, bind_port, atem_address, atem_port);
        else run_program(bind_address, bind_port, atem_address, atem_port);
    }

    return 0;
}
catch(const std::system_error& e)
{
    std::cout << e.what() << " (" << e.code().value() << ")" << std::endl;
    return e.code().value();
}
catch(const std::exception& e)
{
    std::cout << e.what() << std::endl;
    return 1;
}

////////////////////////////////////////////////////////////////////////////////
void run_program(std::string_view bind_address, std::string_view bind_port, std::string_view atem_address, std::string_view atem_port)
{
    asio::io_context ctx;

    asio::signal_set signals{ctx, SIGINT, SIGTERM};
    signals.async_wait([&](auto ec, int signal)
    {
        if(!ec)
        {
            std::cout << "Received signal " << signal << " - exiting" << std::endl;
            ctx.stop();
        }
    });

    run_atemd(ctx, bind_address, bind_port, atem_address, atem_port);
}

////////////////////////////////////////////////////////////////////////////////
void run_service(std::string name, std::string_view bind_address, std::string_view bind_port, std::string_view atem_address, std::string_view atem_port)
{
    asio::io_context ctx;
    std::atomic<bool> done{false};

    win::service service{std::move(name)};
    service.start([&] // run_cb
    {
        for(; !done; ctx.restart())
        try
        {
            std::cout << "Starting service" << std::endl;
            run_atemd(ctx, bind_address, bind_port, atem_address, atem_port);
        }
        catch(const std::exception& e)
        {
            std::cout << e.what() << std::endl;

            std::cout << "Sleeping for 5 seconds..." << std::endl;
            std::this_thread::sleep_for(5s);
        }

        std::cout << "Done" << std::endl;
        return 0;
    },
    [&] // stop_cb
    {
        done = true;
        ctx.stop(); // thread-safe
    });
}
