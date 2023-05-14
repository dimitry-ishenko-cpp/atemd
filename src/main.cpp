////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2022-2023 Dimitry Ishenko
// Contact: dimitry (dot) ishenko (at) (gee) mail (dot) com
//
// Distributed under the GNU GPL license. See the LICENSE.md file for details.

////////////////////////////////////////////////////////////////////////////////
#include "connection.hpp"
#include "pgm/args.hpp"
#include "server.hpp"
#include "win/service.hpp"

#include <asio.hpp>
#include <atem++.hpp>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <tuple>

namespace fs = std::filesystem;

using std::string;
using std::string_view;

using namespace std::chrono_literals;

////////////////////////////////////////////////////////////////////////////////
const string default_bind_address = "0.0.0.0";
const string default_bind_port = "8899";

const string default_atem_port = "9910";

const string default_bind_uri = default_bind_address + ":" + default_bind_port;

////////////////////////////////////////////////////////////////////////////////
auto parse_uri(const string& uri, const string& default_address, const string& default_port)
{
    string address, port;

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
void run_program(string_view bind_address, string_view bind_port, string_view atem_address, string_view atem_port);
void run_service(string name, string_view bind_address, string_view bind_port, string_view atem_address, string_view atem_port);

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
auto parse_cmd(string cmd)
{
    atem::input_id in{ };
    string reply;

    if(cmd.compare(0, 4, "prv=") == 0)
    {
        reply = cmd;

        auto tail = cmd.substr(4);
        cmd.erase(3);

        char* end;
        auto n = std::strtol(tail.data(), &end, 0);

        if(end != tail.data() + tail.size())
        {
            std::cout << "Invalid input # '" << tail << "'" << std::endl;
            in = atem::no_id;
        }
        else in = static_cast<atem::input_id>(n);
    }
    else if(cmd == "ping" || cmd == "auto") reply = "ACK";

    return std::make_tuple(std::move(cmd), in, std::move(reply));
}

////////////////////////////////////////////////////////////////////////////////
void info(const atem::device& device)
{
    std::cout << "Product name: " << device.prod_info() << std::endl;
    std::cout << "Protocol version: " << device.protocol().major << '.' << device.protocol().minor << std::endl;

    std::cout << "Inputs: ";
    for(auto n = 0; n < device.input_count(); ++n)
        std::cout << device.input(n).id() << '-' << device.input(n).name() << ' ';
    std::cout << std::endl;
}

////////////////////////////////////////////////////////////////////////////////
void run_program(string_view bind_address, string_view bind_port, string_view atem_address, string_view atem_port)
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

    atem::device device{ctx, atem_address, atem_port};
    device.on_offline([&]
    {
        std::cout << "Lost connection to ATEM - exiting" << std::endl;
        ctx.stop();
    });

    server server{ctx, bind_address, bind_port};
    std::cout << "Bound to " << bind_address << ":" << bind_port << std::endl;

    server.on_accepted([&](auto socket)
    {
        std::cout << "Accepted connection from " << socket.remote_endpoint() << std::endl;

        auto conn = connection::create(std::move(socket));
        conn->on_received([&](const string& cmd)
        {
            std::cout << "Received: " << cmd << std::endl;

            auto [type, in, reply] = parse_cmd(cmd);

            if(in != atem::no_id && type == "prv") device.me(0).set_pvw(in);
            else if(type == "auto") device.me(0).auto_trans();

            if(reply.size()) std::cout << "Replying: " << reply << std::endl;
            return reply;
        });
        conn->on_message([](const string& msg) { std::cout << msg << std::endl; });

        std::cout << "Waiting for commands" << std::endl;
        conn->start();
    });

    device.on_defined([&]
    {
        std::cout << "Connected to ATEM on " << atem_address << ":" << atem_port << std::endl;
        info(device);

        std::cout << "Listening for connections" << std::endl;
        server.start();
    });

    ctx.run();
}

////////////////////////////////////////////////////////////////////////////////
void run_service(string name, string_view bind_address, string_view bind_port, string_view atem_address, string_view atem_port)
{
    asio::io_context ctx;
    std::atomic<bool> done{false};

    auto run_cb = [&]
    {
        for(; !done; ctx.restart())
        try
        {
            std::cout << "Starting service" << std::endl;

            atem::device device{ctx, atem_address, atem_port};
            device.on_offline([]{ throw std::runtime_error{"Lost connection to ATEM"}; });

            server server{ctx, bind_address, bind_port};
            std::cout << "Bound to " << bind_address << ":" << bind_port << std::endl;

            server.on_accepted([&](auto socket)
            {
                std::cout << "Accepted connection from " << socket.remote_endpoint() << std::endl;

                auto conn = connection::create(std::move(socket));
                conn->on_received([&](const string& cmd)
                {
                    std::cout << "Received: " << cmd << std::endl;

                    auto [type, in, reply] = parse_cmd(cmd);

                    if(in != atem::no_id && type == "prv") device.me(0).set_pvw(in);
                    else if(type == "auto") device.me(0).auto_trans();

                    if(reply.size()) std::cout << "Replying: " << reply << std::endl;
                    return reply;
                });
                conn->on_message([](const string& msg) { std::cout << msg << std::endl; });

                std::cout << "Waiting for commands" << std::endl;
                conn->start();
            });

            device.on_defined([&]
            {
                std::cout << "Connected to ATEM on " << atem_address << ":" << atem_port << std::endl;
                info(device);

                std::cout << "Listening for connections" << std::endl;
                server.start();
            });

            ctx.run();
        }
        catch(const std::exception& e)
        {
            std::cout << e.what() << std::endl;

            std::cout << "Sleeping for 5 seconds..." << std::endl;
            std::this_thread::sleep_for(5s);
        }

        std::cout << "Done" << std::endl;
        return 0;
    };
    auto stop_cb = [&]
    {
        done = true;
        ctx.stop(); // thread-safe
    };

    win::service service{std::move(name)};
    service.start(run_cb, stop_cb);
}
