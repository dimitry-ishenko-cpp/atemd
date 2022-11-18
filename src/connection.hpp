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
#include <chrono>
#include <functional>
#include <memory>
#include <string>

using namespace std::chrono_literals;

////////////////////////////////////////////////////////////////////////////////
class connection : public std::enable_shared_from_this<connection>
{
    using string = std::string;
    using tcp = asio::ip::tcp;

public:
    static auto create(tcp::socket socket)
    {
        return std::shared_ptr<connection>(new connection{ std::move(socket) });
    }

    using recv_cb = std::function<string(const string&)>;
    void on_received(recv_cb cb) { recv_cb_ = std::move(cb); }

    void start() { async_wait(); }

    using msg_cb = std::function<void(const string&)>;
    void on_message(msg_cb cb) { msg_cb_ = std::move(cb); }

private:
    tcp::socket socket_;
    asio::steady_timer timer_;

    recv_cb recv_cb_;
    msg_cb msg_cb_;

    string data_;

    explicit connection(tcp::socket socket) :
        socket_{std::move(socket)}, timer_{socket_.get_executor()}
    { }

    void async_wait()
    {
        // NB: Using shared_from_this() here creates a copy of the
        // shared pointer, which is then bound with the recv() function
        // and passed to asio::io_context using async_await().
        // This keeps the connection alive for as long as it is
        // waiting for data.

        socket_.async_wait(tcp::socket::wait_read,
            std::bind(&connection::recv, shared_from_this(), std::placeholders::_1)
        );

        timer_.expires_after(30s);
        timer_.async_wait([&](asio::error_code ec)
        {
            if(!ec)
            {
                message("Closing connection - timeout");
                socket_.cancel();
            }
        });
    }

    void recv(asio::error_code ec)
    {
        if(!ec)
        {
            string data(socket_.available(), '\0');
            socket_.receive(asio::buffer(data));
            data_ += data;

            for(std::size_t p; (p = data_.find("\r\n")) != data_.npos; )
            {
                auto cmd = data_.substr(0, p);
                data_.erase(0, p + 2);

                if(recv_cb_) if(auto reply = recv_cb_(cmd); reply.size())
                {
                    asio::error_code ec;
                    socket_.send(asio::buffer(reply + "\r\n"), { }, ec);
                    if(ec)
                    {
                        message("Send error: " + ec.message());
                        return;
                    }
                }
            }

            async_wait();
        }
    }

    void message(const string& s) { if(msg_cb_) msg_cb_(s); }
};

////////////////////////////////////////////////////////////////////////////////
#endif