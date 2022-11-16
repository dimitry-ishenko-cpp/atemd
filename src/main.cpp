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
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include "connection.hpp"
#include "pgm/args.hpp"
#include "server.hpp"

using std::string;
using std::vector;
using weak_connection = std::weak_ptr<connection>;

////////////////////////////////////////////////////////////////////////////////
const string default_bind_address = "0.0.0.0";
const string default_bind_port = "1230";

const string default_bind_uri = default_bind_address + ":" + default_bind_port;

const string default_atem_port = "9910";

////////////////////////////////////////////////////////////////////////////////
auto parse_uri(const string& uri, const string& default_address, const string& default_port)
{
    string address, port;

    if(auto p = uri.find(':'); p != uri.npos)
    {
        address = uri.substr(0, p);
        port = uri.substr(p + 1);

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
    std::cout << "Product name: " << device.prod_info() << std::endl;
    std::cout << "Protocol version: " << device.protocol().major << '.' << device.protocol().minor << std::endl;

    std::cout << "Inputs: ";
    for(auto n = 0; n < device.input_count(); ++n)
        std::cout << device.input(n).id() << '-' << device.input(n).name() << ' ';
    std::cout << std::endl;
}

////////////////////////////////////////////////////////////////////////////////
auto parse_cmd(string cmd)
{
    atem::input_id in{ };

    if(cmd.compare(0, 3, "pg_") == 0 || cmd.compare(0, 3, "pv_") == 0)
    {
        auto tail = cmd.substr(3);
        cmd.erase(2);

        char* end;
        auto n = std::strtol(tail.data(), &end, 0);

        if(end != tail.data() + tail.size())
        {
            std::cout << "Invalid input # '" << tail << "'" << std::endl;
            in = atem::no_id;
        }
        else in = static_cast<atem::input_id>(n);
    }

    return std::make_tuple(std::move(cmd), in);
}

////////////////////////////////////////////////////////////////////////////////
void notify_all(vector<weak_connection>& active, const string& cmd)
{
    for(auto it = active.begin(); it != active.end(); )
    {
        if(auto conn = it->lock())
        {
            try { conn->send(cmd + '\n'); }
            catch(const std::exception& e)
            {
                std::cout << "Error: " << e.what() << std::endl;
                conn->stop();
            }
            it++;
        }
        else
        {
            std::cout << "Removing inactive connection" << std::endl;
            it = active.erase(it);
        }
    }
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

        asio::signal_set signals{ctx, SIGINT, SIGTERM};
        signals.async_wait([&](auto ec, int signal)
        {
            if(!ec)
            {
                std::cout << "Received signal " << signal << " - exiting" << std::endl;
                ctx.stop();
            }
        });

        atem::device device{ctx, remote_address, remote_port};
        device.on_offline([&]
        {
            std::cout << "Lost connection to ATEM - exiting";
            ctx.stop();
        });

        server server{ctx, local_address, local_port};
        std::cout << "Bound to " << local_address << ":" << local_port << std::endl;

        vector<weak_connection> active;

        server.on_accepted([&](auto socket)
        {
            std::cout << "Accepted connection from " << socket.remote_endpoint() << std::endl;

            auto conn = connection::create(std::move(socket));
            active.emplace_back(conn);

            conn->on_received([&](const string& cmd)
            {
                std::cout << "Received: " << cmd << std::endl;

                auto [ch, in] = parse_cmd(cmd);
                     if(ch == "tr") device.me(0).auto_trans();
                else if(ch == "ct") device.me(0).cut();
                else if(in != atem::no_id)
                {
                         if(ch == "pg") device.me(0).set_pgm(in);
                    else if(ch == "pv") device.me(0).set_pvw(in);
                }
            });

            std::cout << "Waiting for commands" << std::endl;
            conn->start();
        });

        device.on_defined([&]
        {
            std::cout << "Connected to ATEM on " << remote_address << ":" << remote_port << std::endl;
            show_info(device);

            device.me(0).on_pgm_changed([&](auto in)
            {
                auto cmd = "pg=" + std::to_string(in);
                std::cout << "Notifying: " << cmd << std::endl;

                notify_all(active, cmd); 
            });

            device.me(0).on_pvw_changed([&](auto in)
            {
                auto cmd = "pv=" + std::to_string(in);
                std::cout << "Notifying: " << cmd << std::endl;

                notify_all(active, cmd);
            });

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