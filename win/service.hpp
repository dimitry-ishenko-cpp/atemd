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

    static void start(std::string name, run_cb, stop_cb);

private:
    static std::string name_;

    static run_cb run_cb_;
    static stop_cb stop_cb_;

    static SERVICE_STATUS status_;
    static SERVICE_STATUS_HANDLE handle_;

    static void WINAPI main(DWORD argc, char* argv[]);
    static void WINAPI control(DWORD);

    static void set_running();
    static void set_stop_pending();
    static void set_stopped(int exit_code);
};

////////////////////////////////////////////////////////////////////////////////
}

////////////////////////////////////////////////////////////////////////////////
#endif
