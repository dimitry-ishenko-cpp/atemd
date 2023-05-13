////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023 Dimitry Ishenko
// Contact: dimitry (dot) ishenko (at) (gee) mail (dot) com
//
// Distributed under the GNU GPL license. See the LICENSE.md file for details.

////////////////////////////////////////////////////////////////////////////////
#include "server.hpp"

#include <asio.hpp>
#include <atem++.hpp>
#include <string_view>

////////////////////////////////////////////////////////////////////////////////
class atemd
{
public:
    atemd(std::string_view bind_address, std::string_view bind_port, std::string_view atem_address, std::string_view atem_port);

    void run() { ctx_.run(); }
    void stop() { ctx_.stop(); }

private:
    asio::io_context ctx_;
    asio::signal_set signals_;

    atem::device device_;
    server server_;

    void show_info();
};

////////////////////////////////////////////////////////////////////////////////
