////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023 Dimitry Ishenko
// Contact: dimitry (dot) ishenko (at) (gee) mail (dot) com
//
// Distributed under the GNU GPL license. See the LICENSE.md file for details.

////////////////////////////////////////////////////////////////////////////////
#include "atemd.hpp"
#include "connection.hpp"
#include "server.hpp"

#include <atem++.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>

////////////////////////////////////////////////////////////////////////////////
namespace
{

auto parse_cmd(std::string cmd)
{
    atem::input_id in{ };
    if(cmd.compare(0, 4, "prv=") == 0)
    {
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
    return std::make_tuple(std::move(cmd), in);
}

void info(const atem::device& device)
{
    std::cout << "Product name: " << device.prod_info() << std::endl;
    std::cout << "Protocol version: " << device.protocol().major << '.' << device.protocol().minor << std::endl;

    std::cout << "Inputs: ";
    for(auto n = 0; n < device.input_count(); ++n)
        std::cout << device.input(n).id() << '-' << device.input(n).name() << ' ';
    std::cout << std::endl;
}

}

////////////////////////////////////////////////////////////////////////////////
void run_atemd(asio::io_context& ctx, std::string_view bind_address, std::string_view bind_port, std::string_view atem_address, std::string_view atem_port)
{
    atem::device device{ctx, atem_address, atem_port};
    device.on_offline([]{ throw std::runtime_error{"Lost connection to ATEM"}; });

    server server{ctx, bind_address, bind_port};
    std::cout << "Bound to " << bind_address << ":" << bind_port << std::endl;

    server.on_accepted([&](auto socket)
    {
        std::cout << "Accepted connection from " << socket.remote_endpoint() << std::endl;

        auto conn = connection::create(std::move(socket));
        conn->on_received([&](const std::string& cmd)
        {
            std::cout << "Received: " << cmd << std::endl;

            auto [type, in] = parse_cmd(cmd);
            std::string reply;

            if(type == "auto")
            {
                device.me(0).auto_trans();
                reply = "ACK";
            }
            else if(type == "ping")
            {
                reply = "ACK";
            }
            else if(in != atem::no_id && type == "prv") 
            {
                device.me(0).set_pvw(in);
                reply = cmd;
            }

            if(reply.size()) std::cout << "Replying: " << reply << std::endl;
            return reply;
        });
        conn->on_message([](const std::string& msg) { std::cout << msg << std::endl; });

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
