////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023 Dimitry Ishenko
// Contact: dimitry (dot) ishenko (at) (gee) mail (dot) com
//
// Distributed under the GNU GPL license. See the LICENSE.md file for details.

////////////////////////////////////////////////////////////////////////////////
#ifndef WIN_SERVICE_HPP
#define WIN_SERVICE_HPP

#include <functional>
#include <string>
#include <windows.h>

////////////////////////////////////////////////////////////////////////////////
namespace win
{

////////////////////////////////////////////////////////////////////////////////
class service
{
public:
    using run_cb = std::function<int()>;
    using stop_cb = std::function<void()>;

    service(std::string name);

    void start(run_cb, stop_cb);

private:
    std::string name_;

    run_cb run_cb_;
    stop_cb stop_cb_;

    SERVICE_STATUS status_{ }; // zero-initialized
    SERVICE_STATUS_HANDLE handle_ = nullptr;

    void set_running();
    void set_stop_pending();
    void set_stopped(int exit_code);

    ////////////////////
    static service* ctx_;
    static void WINAPI main(DWORD argc, char* argv[]);
    static DWORD WINAPI control(DWORD, DWORD, LPVOID, LPVOID);
};

////////////////////////////////////////////////////////////////////////////////
}

////////////////////////////////////////////////////////////////////////////////
#endif
