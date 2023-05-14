////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2023 Dimitry Ishenko
// Contact: dimitry (dot) ishenko (at) (gee) mail (dot) com
//
// Distributed under the GNU GPL license. See the LICENSE.md file for details.

////////////////////////////////////////////////////////////////////////////////
#include "service.hpp"

#include <iostream>
#include <system_error>

////////////////////////////////////////////////////////////////////////////////
namespace win
{

////////////////////////////////////////////////////////////////////////////////
class windows_error : public std::system_error
{
public:
    explicit windows_error(const std::string& msg) :
        std::system_error{static_cast<int>(GetLastError()), std::system_category(), msg}
    { }
};

////////////////////////////////////////////////////////////////////////////////
service* service::ctx_ = nullptr;

////////////////////////////////////////////////////////////////////////////////
service::service(std::string name) :
    name_{std::move(name)}
{
    status_.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status_.dwControlsAccepted = SERVICE_ACCEPT_STOP;
}

////////////////////////////////////////////////////////////////////////////////
void service::start(run_cb run_cb, stop_cb stop_cb)
{
    run_cb_ = std::move(run_cb);
    stop_cb_ = std::move(stop_cb);

    SERVICE_TABLE_ENTRY table[] =
    {
        { name_.data(), &service::main },
        { nullptr, nullptr }
    };
    ctx_ = this;

    if(!StartServiceCtrlDispatcher(table)) throw windows_error{"StartServiceCtrlDispatcher"};
}

////////////////////////////////////////////////////////////////////////////////
void WINAPI service::main(DWORD argc, char* argv[])
{
    auto srv = ctx_;
    try
    {
        srv->handle_ = RegisterServiceCtrlHandlerEx(srv->name_.data(), &service::control, srv);
        if(!srv->handle_) throw windows_error{"RegisterServiceCtrlHandler"};

        srv->set_running();
        if(srv->run_cb_) srv->run_cb_();
        srv->set_stopped(0);
    }
    catch(const windows_error& e)
    {
        std::cout << e.what() << std::endl;
        srv->set_stopped(e.code().value());
    }
    catch(const std::exception& e)
    {
        std::cout << e.what() << std::endl;
        srv->set_stopped(ERROR_INVALID_FUNCTION);
    }
}

////////////////////////////////////////////////////////////////////////////////
DWORD WINAPI service::control(DWORD state, DWORD, LPVOID, LPVOID ctx)
{
    if(state == SERVICE_CONTROL_STOP)
    {
        auto srv = static_cast<service*>(ctx);
        srv->set_stop_pending();
        if(srv->stop_cb_) srv->stop_cb_();
    }
    return NO_ERROR;
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
