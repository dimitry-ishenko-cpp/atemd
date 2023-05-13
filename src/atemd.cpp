////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023 Dimitry Ishenko
// Contact: dimitry (dot) ishenko (at) (gee) mail (dot) com
//
// Distributed under the GNU GPL license. See the LICENSE.md file for details.

////////////////////////////////////////////////////////////////////////////////
#include "atemd.hpp"
#include "connection.hpp"

#include <iostream>
#include <string>

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

}

////////////////////////////////////////////////////////////////////////////////
atemd::atemd(std::string_view bind_address, std::string_view bind_port, std::string_view atem_address, std::string_view atem_port) :
    signals_{ctx_, SIGINT, SIGTERM},
    device_ {ctx_, atem_address, atem_port},
    server_ {ctx_, bind_address, bind_port}
{
    std::cout << "Bound to " << bind_address << ":" << bind_port << std::endl;

    signals_.async_wait([=](auto ec, int signal)
    {
        if(!ec)
        {
            std::cout << "Received signal " << signal << " - exiting" << std::endl;
            stop();
        }
    });

    device_.on_offline([=]
    {
        std::cout << "Lost connection to ATEM - exiting";
        stop();
    });

    device_.on_defined([=]
    {
        std::cout << "Connected to ATEM on " << atem_address << ":" << atem_port << std::endl;
        show_info();

        std::cout << "Listening for connections" << std::endl;
        server_.start();
    });

    server_.on_accepted([=](auto socket)
    {
        std::cout << "Accepted connection from " << socket.remote_endpoint() << std::endl;

        auto conn = connection::create(std::move(socket));
        conn->on_received([=](const std::string& cmd) -> auto
        {
            std::cout << "Received: " << cmd << std::endl;

            auto [ch, in] = parse_cmd(cmd);
            std::string reply;

            if(ch == "auto")
            {
                device_.me(0).auto_trans();
                reply = "ACK";
            }
            else if(ch == "ping")
            {
                reply = "ACK";
            }
            else if(in != atem::no_id && ch == "prv") 
            {
                device_.me(0).set_pvw(in);
                reply = cmd;
            }

            if(reply.size()) std::cout << "Replying: " << reply << std::endl;
            return reply;
        });

        conn->on_message([](const std::string& msg) { std::cout << msg << std::endl; });

        std::cout << "Waiting for commands" << std::endl;
        conn->start();
    });
}

////////////////////////////////////////////////////////////////////////////////
void atemd::show_info()
{
    std::cout << "Product name: " << device_.prod_info() << std::endl;
    std::cout << "Protocol version: " << device_.protocol().major << '.' << device_.protocol().minor << std::endl;

    std::cout << "Inputs: ";
    for(auto n = 0; n < device_.input_count(); ++n)
        std::cout << device_.input(n).id() << '-' << device_.input(n).name() << ' ';
    std::cout << std::endl;
}
