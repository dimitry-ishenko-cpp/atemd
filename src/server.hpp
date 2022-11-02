////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2022 Dimitry Ishenko
// Contact: dimitry (dot) ishenko (at) (gee) mail (dot) com
//
// Distributed under the GNU GPL license. See the LICENSE.md file for details.

////////////////////////////////////////////////////////////////////////////////
#ifndef SERVER_HPP
#define SERVER_HPP

////////////////////////////////////////////////////////////////////////////////
#include <asio.hpp>
#include <functional>
#include <string>
#include <string_view>

////////////////////////////////////////////////////////////////////////////////
class server
{
    using string = std::string;
    using string_view = std::string_view;
    using tcp = asio::ip::tcp;

public:
    server(asio::io_context& ctx, string_view address, string_view port) :
        acceptor_{ctx, make_endpoint(address, port)}
    { }

    void start() { async_accept(); }

    using accept_cb = std::function<void(tcp::socket)>;
    void on_accepted(accept_cb cb) { accept_cb_ = std::move(cb); }

private:
    tcp::acceptor acceptor_;
    accept_cb accept_cb_;

    void async_accept()
    {
        acceptor_.async_accept([=](auto ec, tcp::socket sock)
        {
            if(!ec)
            {
                if(accept_cb_) accept_cb_(std::move(sock));
                async_accept();
            }
        });
    }

    tcp::endpoint make_endpoint(string_view address, string_view port)
    {
        asio::error_code ec;
        auto address_ = asio::ip::make_address(address, ec);

        if(ec) throw std::invalid_argument{
            "Invalid address '" + string{address} + "'"
        };

        char* end;
        unsigned short port_ = std::strtol(port.data(), &end, 0);

        if(end != port.data() + port.size()) throw std::invalid_argument{
            "Invalid port # '" + string{port} + "'"
        };

        return tcp::endpoint{address_, port_};
    }
};

////////////////////////////////////////////////////////////////////////////////
#endif