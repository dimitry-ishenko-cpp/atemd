////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2022 Dimitry Ishenko
// Contact: dimitry (dot) ishenko (at) (gee) mail (dot) com
//
// Distributed under the GNU GPL license. See the LICENSE.md file for details.

////////////////////////////////////////////////////////////////////////////////
#ifndef CONNECTION_HPP
#define CONNECTION_HPP

////////////////////////////////////////////////////////////////////////////////
#include <asio.hpp>
#include <functional>
#include <memory>
#include <string>

////////////////////////////////////////////////////////////////////////////////
class connection : public std::enable_shared_from_this<connection>
{
    using string = std::string;
    using tcp = asio::ip::tcp;

public:
    static auto create(tcp::socket sock)
    {
        return std::shared_ptr<connection>(new connection{std::move(sock)});
    }

    using recv_cb = std::function<void(string)>;
    void on_recv(recv_cb cb) { recv_cb_ = std::move(cb); }

    void start() { async_wait(); }

private:
    tcp::socket socket_;
    recv_cb recv_cb_;
    string data_;

    explicit connection(tcp::socket sock) : socket_{std::move(sock)} { }

    void async_wait()
    {
        // NB: Using shared_from_this() creates a copy of the
        // shared pointer, which is then bound with the recv() function
        // and passed to asio::io_context using async_await().
        // This keeps the connection alive for as long as it is
        // waiting for data.

        socket_.async_wait(tcp::socket::wait_read,
            std::bind(&connection::recv, shared_from_this(), std::placeholders::_1)
        );
    }

    void recv(asio::error_code ec)
    {
        if(!ec)
        {
            string data(socket_.available(), '\0');
            socket_.receive(asio::buffer(data));
            data_ += data;

            for(std::size_t p; (p = data_.find('\n')) != data_.npos; )
            {
                auto cmd = data_.substr(0, p);
                if(cmd.back() == '\r') cmd.pop_back();
                data_.erase(0, p + 1);

                // ACK each command
                socket_.send(asio::buffer("ACK\r\n"));

                if(recv_cb_) recv_cb_(std::move(cmd));
            }

            async_wait();
        }
    }
};

////////////////////////////////////////////////////////////////////////////////
#endif