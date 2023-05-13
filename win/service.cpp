////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023 Dimitry Ishenko
// Contact: dimitry (dot) ishenko (at) (gee) mail (dot) com
//
// Distributed under the GNU GPL license. See the LICENSE.md file for details.

////////////////////////////////////////////////////////////////////////////////
#include "service.hpp"

#include <string>
#include <system_error>

////////////////////////////////////////////////////////////////////////////////
namespace win
{

////////////////////////////////////////////////////////////////////////////////
class windows_error : public std::system_error
{
public:
    explicit windows_error(const std::string& msg) :
        std::system_error{std::error_code{ int(GetLastError()), std::system_category() }, msg}
    { }
};

////////////////////////////////////////////////////////////////////////////////
std::string service::name_;

service::run_cb service::run_cb_;
service::stop_cb service::stop_cb_;

SERVICE_STATUS service::status_
{
    SERVICE_WIN32_OWN_PROCESS,
    0, // dwCurrentState
    SERVICE_ACCEPT_STOP,
    0, // dwWin32ExitCode
    0, // dwServiceSpecificExitCode
    0, // dwCheckPoint
    0, // dwWaitHint
};

SERVICE_STATUS_HANDLE service::handle_ = nullptr;

////////////////////////////////////////////////////////////////////////////////
void service::start(std::string name, run_cb run_cb, stop_cb stop_cb)
{
    name_ = std::move(name);

    run_cb_ = std::move(run_cb);
    stop_cb_ = std::move(stop_cb);

    SERVICE_TABLE_ENTRY table[] =
    {
        { name_.data(), &service::main },
        { nullptr, nullptr }
    };

    if(!StartServiceCtrlDispatcher(table)) throw windows_error{"StartServiceCtrlDispatcher"};
}

////////////////////////////////////////////////////////////////////////////////
void WINAPI service::main(DWORD argc, char* argv[])
try
{
    handle_ = RegisterServiceCtrlHandler(name_.data(), &service::control);
    if(!handle_) throw windows_error{"RegisterServiceCtrlHandler"};

    set_running();
    auto exit_code = run_cb_();
    set_stopped(exit_code);
}
catch(const std::system_error& e)
{
    set_stopped(e.code().value());
}

////////////////////////////////////////////////////////////////////////////////
void WINAPI service::control(DWORD state)
{
    if(state == SERVICE_CONTROL_STOP)
    {
        set_stop_pending();
        stop_cb_();
    }
}

////////////////////////////////////////////////////////////////////////////////
void service::set_running()
{
    status_.dwCurrentState = SERVICE_RUNNING;
    status_.dwWin32ExitCode = 0;
    SetServiceStatus(handle_, &status_);
}

////////////////////////////////////////////////////////////////////////////////
void service::set_stop_pending()
{
    status_.dwCurrentState = SERVICE_STOP_PENDING;
    status_.dwWin32ExitCode = 0;
    SetServiceStatus(handle_, &status_);
}

////////////////////////////////////////////////////////////////////////////////
void service::set_stopped(int exit_code)
{
    status_.dwCurrentState = SERVICE_STOPPED;
    status_.dwWin32ExitCode = exit_code;
    SetServiceStatus(handle_, &status_);
}

////////////////////////////////////////////////////////////////////////////////
}
