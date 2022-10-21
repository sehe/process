
// Copyright (c) 2022 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#ifndef BOOST_PROCESS_V2_DETAIL_PROCESS_HANDLE_FD_OR_SIGNAL_HPP
#define BOOST_PROCESS_V2_DETAIL_PROCESS_HANDLE_FD_OR_SIGNAL_HPP

#include <boost/process/v2/detail/config.hpp>

#include <sys/types.h>
#include <sys/wait.h>

#include <boost/process/v2/detail/last_error.hpp>
#include <boost/process/v2/detail/throw_error.hpp>
#include <boost/process/v2/exit_code.hpp>
#include <boost/process/v2/pid.hpp>

#if defined(BOOST_PROCESS_V2_STANDALONE)
#include <asio/any_io_executor.hpp>
#include <asio/compose.hpp>
#include <asio/dispatch.hpp>
#include <asio/posix/basic_stream_descriptor.hpp>
#include <asio/post.hpp>
#include <asio/windows/signal_set.hpp>
#else
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/posix/basic_stream_descriptor.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/signal_set.hpp>
#endif

BOOST_PROCESS_V2_BEGIN_NAMESPACE

namespace detail
{

template<typename Executor = BOOST_PROCESS_V2_ASIO_NAMESPACE::any_io_executor>
struct basic_process_handle_fd_or_signal
{
    using native_handle_type = int;
    
    typedef Executor executor_type;

    executor_type get_executor()
    { return signal_set_.get_executor(); }

    /// Rebinds the process_handle to another executor.
    template<typename Executor1>
    struct rebind_executor
    {
        /// The socket type when rebound to the specified executor.
        typedef basic_process_handle_fd_or_signal<Executor1> other;
    };

    template<typename ExecutionContext>
    basic_process_handle_fd_or_signal(ExecutionContext &context,
                                typename std::enable_if<
                                        std::is_convertible<ExecutionContext &,
                                                BOOST_PROCESS_V2_ASIO_NAMESPACE::execution_context &>::value
                                >::type * = nullptr)
            : pid_(-1), descriptor_(context)
    {
    }

    template<typename ExecutionContext>
    basic_process_handle_fd_or_signal(ExecutionContext &context,
                                      pid_type pid,
                                      typename std::enable_if<
                                          std::is_convertible<ExecutionContext &,
                                              BOOST_PROCESS_V2_ASIO_NAMESPACE::execution_context &>::value
                                          >::type * = nullptr)
            : pid_(pid), descriptor_(context)
    {
    }
    
    template<typename ExecutionContext>
    basic_process_handle_fd_or_signal(ExecutionContext &context,
                                      pid_type pid, native_handle_type process_handle,
                                      typename std::enable_if<
                                          std::is_convertible<ExecutionContext &,
                                              BOOST_PROCESS_V2_ASIO_NAMESPACE::execution_context &>::value
                                          >::type * = nullptr)
            : pid_(pid), descriptor_(context, process_handle)
    {
    }
    

    basic_process_handle_fd_or_signal(Executor executor)
            : pid_(-1), descriptor_(executor)
    {
    }

    basic_process_handle_fd_or_signal(Executor executor, pid_type pid)
            : pid_(pid), descriptor_(executor)
    {
    }

    basic_process_handle_fd_or_signal(Executor executor, pid_type pid, native_handle_type process_handle)
            : pid_(pid), descriptor_(executor, process_handle)
    {
    }


    basic_process_handle_fd_or_signal(basic_process_handle_fd_or_signal &&handle)
            : pid_(handle.pid_), descriptor_(std::move(handle.descriptor_))
    {
        handle.pid_ = -1;
    }
    // Warn: does not change the executor of the signal-set.
    basic_process_handle_fd_or_signal& operator=(basic_process_handle_fd_or_signal &&handle)
    {
        pid_ = handle.pid_;
        descriptor_ = std::move(handle.descriptor_);
        handle.pid_ = -1;
        return *this;
    }


    template<typename Executor1>
    basic_process_handle_fd_or_signal(basic_process_handle_fd_or_signal<Executor1> &&handle)
            : pid_(handle.pid_), descriptor_(std::move(handle.descriptor_))
    {
        handle.pid_ = -1;
    }

    pid_type id() const
    { return pid_; }

    void terminate_if_running(error_code &)
    {
        if (pid_ <= 0)
            return;
        if (::waitpid(pid_, nullptr, WNOHANG) == 0)
        {
            ::kill(pid_, SIGKILL);
            ::waitpid(pid_, nullptr, 0);
        }
    }

    void terminate_if_running()
    {
        if (pid_ <= 0)
            return;
        if (::waitpid(pid_, nullptr, WNOHANG) == 0)
        {
            ::kill(pid_, SIGKILL);
            ::waitpid(pid_, nullptr, 0);
        }
    }

    void wait(native_exit_code_type &exit_status, error_code &ec)
    {
        printf("WAIT: %d %d\n", pid_, descriptor_.native_handle());

        if (pid_ <= 0)
            return;

        int res = 0;
        while ((res = ::waitpid(pid_, &exit_status, 0)) < 0)
        {
            printf("WAIT: %d %d %d\n", res, errno, exit_status);
            if (errno != EINTR)
            {
                ec = get_last_error();
                break;
            }               
        }
        printf("WAIT: %d %d %d\n", res, errno, exit_status);

    }

    void wait(native_exit_code_type &exit_status)
    {
        if (pid_ <= 0)
            return;
        error_code ec;
        wait(exit_status, ec);
        if (ec)
            detail::throw_error(ec, "wait(pid)");
    }

    void interrupt(error_code &ec)
    {
        if (pid_ <= 0)
            return;
        if (::kill(pid_, SIGINT) == -1)
            ec = get_last_error();
    }

    void interrupt()
    {
        if (pid_ <= 0)
            return;
        error_code ec;
        interrupt(ec);
        if (ec)
            detail::throw_error(ec, "interrupt");
    }

    void request_exit(error_code &ec)
    {
        if (pid_ <= 0)
            return;
        if (::kill(pid_, SIGTERM) == -1)
            ec = get_last_error();
    }

    void request_exit()
    {
        if (pid_ <= 0)
            return;
        error_code ec;
        request_exit(ec);
        if (ec)
            detail::throw_error(ec, "request_exit");
    }

    void terminate(native_exit_code_type &exit_status, error_code &ec)
    {
        if (pid_ <= 0)
            return;
        if (::kill(pid_, SIGKILL) == -1)
            ec = get_last_error();
    }

    void terminate(native_exit_code_type &exit_status)
    {
        if (pid_ <= 0)
            return;
        error_code ec;
        terminate(exit_status, ec);
        if (ec)
            detail::throw_error(ec, "terminate");
    }

    bool running(native_exit_code_type &exit_code, error_code & ec)
    {
        if (pid_ <= 0)
            return false;
        int code = 0;
        int res = ::waitpid(pid_, &code, WNOHANG);
        if (res == -1)
            ec = get_last_error();
        else
            ec.clear();

        if (process_is_running(res))
            return true;
        else
        {
            exit_code = code;
            return false;
        }
    }

    bool running(native_exit_code_type &exit_code)
    {
        if (pid_ <= 0)
            return false;

        error_code ec;
        bool res = running(exit_code, ec);
        if (ec)
            detail::throw_error(ec, "is_running");
        return res;
    }

    bool is_open() const
    {
        return pid_ != -1;
    }

    template<BOOST_PROCESS_V2_COMPLETION_TOKEN_FOR(void(error_code, int))
    WaitHandler BOOST_PROCESS_V2_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_PROCESS_V2_INITFN_AUTO_RESULT_TYPE(WaitHandler, void (error_code, native_exit_code_type))
    async_wait(WaitHandler &&handler BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        return BOOST_PROCESS_V2_ASIO_NAMESPACE::async_compose<WaitHandler, void(error_code, native_exit_code_type)>(
                async_wait_op_{descriptor_, signal_set_, pid_}, handler, descriptor_);
    }

  private:
    template<typename>
    friend
    struct basic_process_handle_fd_or_signal;
    pid_type pid_ = -1;
    BOOST_PROCESS_V2_ASIO_NAMESPACE::posix::basic_stream_descriptor<Executor> descriptor_;
    BOOST_PROCESS_V2_ASIO_NAMESPACE::basic_signal_set<Executor> signal_set_{descriptor_.get_executor(), SIGCHLD};

    struct async_wait_op_
    {
        BOOST_PROCESS_V2_ASIO_NAMESPACE::posix::basic_descriptor<Executor> &descriptor;
        BOOST_PROCESS_V2_ASIO_NAMESPACE::basic_signal_set<Executor> &handle;
        pid_type pid_;
        bool needs_post = true;

        template<typename Self>
        void operator()(Self &&self, error_code ec = {}, int = 0)
        {
            native_exit_code_type exit_code = -1;
            int wait_res = -1;
            if (pid_ <= 0) // error, complete early
                ec = BOOST_PROCESS_V2_ASIO_NAMESPACE::error::bad_descriptor;
            else 
            {
                ec.clear();
                wait_res = ::waitpid(pid_, &exit_code, WNOHANG);
                if (wait_res == -1)
                    ec = get_last_error();
            }

            if (!ec && (wait_res == 0))
            {
                needs_post = false;
                if (descriptor.is_open())
                    descriptor.async_wait(
                            BOOST_PROCESS_V2_ASIO_NAMESPACE::posix::descriptor_base::wait_read, 
                            std::move(self));
                else
                    handle.async_wait(std::move(self));
                return;
            }

            struct completer
            {
                error_code ec;
                native_exit_code_type code;
                typename std::decay<Self>::type self;

                void operator()()
                {
                    self.complete(ec, code);
                }
            };

            const auto exec = self.get_executor();
            completer cpl{ec, exit_code, std::move(self)};
            if (needs_post)
                BOOST_PROCESS_V2_ASIO_NAMESPACE::post(exec, std::move(cpl));
            else
                BOOST_PROCESS_V2_ASIO_NAMESPACE::dispatch(exec, std::move(cpl));

        }
    };
};
}

BOOST_PROCESS_V2_END_NAMESPACE

#endif //BOOST_PROCESS_HANDLE_FD_OR_SIGNAL_HPP
