////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2022 Dimitry Ishenko
// Contact: dimitry (dot) ishenko (at) (gee) mail (dot) com
//
// Distributed under the GNU GPL license. See the LICENSE.md file for details.

////////////////////////////////////////////////////////////////////////////////
#include <asio.hpp>
#include <atem++.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>

#include "connection.hpp"
#include "pgm/args.hpp"
#include "server.hpp"

using std::string;

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
void show_info(const atem::device& device)
{
    std::cout << "Name: " << device.prod_info() << std::endl;
    std::cout << "Protocol version: " << device.protocol().major << '.' << device.protocol().minor << std::endl;
    std::cout << "# of M/Es: " << device.me_count() << std::endl;
    std::cout << "# of inputs: " << device.input_count() << std::endl;

    for(auto n = 0; n < device.input_count(); ++n)
        std::cout << device.input(n).id() << ' ' << device.input(n).name() << std::endl;
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
        auto local = args["--bind-to"].value_or(default_bind_uri);
        auto [local_address, local_port] = parse_uri(local, default_bind_address, default_bind_port);

        auto remote = args["atem-uri"].value();
        string remote_address, remote_port;
        std::tie(remote_address, remote_port) = parse_uri(remote, "", default_atem_port);

        asio::io_context ctx;

        asio::signal_set signals(ctx, SIGINT, SIGTERM);
        signals.async_wait([&](auto ec, int signal)
        {
            if(!ec)
            {
                std::cout << "Received signal " << signal << ", exiting" << std::endl;
                ctx.stop();
            }
        });

        atem::device device{ctx, remote_address, remote_port};
        device.on_offline([]{ throw std::runtime_error{"Lost connection to ATEM"}; });

        server server{ctx, local_address, local_port};
        std::cout << "Bound to " << local_address << ":" << local_port << std::endl;

        server.on_accepted([&](auto sock)
        {
            std::cout << "Accepted connection from " << sock.remote_endpoint() << std::endl;

            auto conn = connection::create(std::move(sock));
            conn->on_recv([&](std::string cmd)
            {
                std::cout << "Received: " << cmd << std::endl;

                if(cmd.compare(0, 4, "prv=") == 0)
                {
                    cmd.erase(0, 4);
                    char* end;
                    auto in = std::strtol(cmd.data(), &end, 0);

                    if(end == cmd.data() + cmd.size())
                        device.me(0).set_pvw(static_cast<atem::input_id>(in));
                    else std::cout << "Invalid input # '" << cmd << "'" << std::endl;
                }
                else if(cmd == "auto") device.me(0).auto_trans();
            });

            std::cout << "Waiting for commands" << std::endl;
            conn->start();
        });

        device.on_defined([&]
        {
            std::cout << "Connected to ATEM on " << remote_address << ":" << remote_port << std::endl;
            show_info(device);

            std::cout << "Listening for connections" << std::endl;
            server.start();
        });

        ctx.run();
    }

    return 0;
}
catch(const std::exception& e)
{
    std::cerr << e.what() << std::endl;
    return 1;
}
