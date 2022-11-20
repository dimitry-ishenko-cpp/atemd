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
#include <string>

////////////////////////////////////////////////////////////////////////////////
class connection
{
    using string = std::string;
    using tcp = asio::ip::tcp;

public:
    explicit connection(tcp::socket socket) : socket_{std::move(socket)} { async_wait(); }

    using recv_cb = std::function<void(const string&)>;
    void on_received(recv_cb cb) { recv_cb_ = std::move(cb); }

    auto send(const string& data)
    {
        asio::error_code ec;
        socket_.send(asio::buffer(data + '\n'), { }, ec);
        return ec;
    }

private:
    tcp::socket socket_;
    recv_cb recv_cb_;
    string data_;

    void async_wait()
    {
        socket_.async_wait(tcp::socket::wait_read, [=](auto ec)
        {
            if(!ec)
            {
                string data(socket_.available(), '\0');
                socket_.receive(asio::buffer(data));
                data_ += data;

                for(std::size_t p; (p = data_.find('\n')) != data_.npos; )
                {
                    auto cmd = data_.substr(0, p);
                    data_.erase(0, p + 1);

                    if(recv_cb_) recv_cb_(cmd);
                }

                async_wait();
            }
        });
    }
};

////////////////////////////////////////////////////////////////////////////////
#endif