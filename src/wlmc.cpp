#include <thread>
#include <filesystem>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/poll.h>

#include <gsl/util>
#include <spdlog/spdlog.h>
#include "control_flow.hpp"


import observer;

constexpr auto runtime_dir_var = "XDG_RUNTIME_DIR";
constexpr auto wayland_display_var = "WAYLAND_DISPLAY";
constexpr auto socket_listen_queue_len = 10;
constexpr auto acceptor_poll_timeout = 1;

namespace fs = std::filesystem;

std::optional<fs::path> get_server_soket_path()
{
    const auto display = std::string_view(std::getenv(wayland_display_var));
    if (!display.empty() && display.front() == '/')
    {
        spdlog::info("Full path in env: {}", display);
        return fs::path{display};
    }

    const auto runtime_dir = std::string_view(std::getenv(runtime_dir_var));
    if (runtime_dir.empty())
    {
        spdlog::error("{} is empty", runtime_dir_var);
        return std::nullopt;
    }

    return fs::path{runtime_dir} / display;
}

std::optional<fs::path> get_child_socket_path()
{
    const auto runtime_dir = std::string_view(std::getenv(runtime_dir_var));
    if (runtime_dir.empty())
    {
        spdlog::error("{} is empty", runtime_dir_var);
        return std::nullopt;
    }

    return fs::path{runtime_dir} / std::format("wlmc-{}", ::getpid());
}


int main(int argc, char** argv)
{
    auto server_soket_path = get_server_soket_path();
    if (!server_soket_path)
    {
        spdlog::error("Cannot resolve wayland server socket");
        return EXIT_FAILURE;
    }
    spdlog::info("Server socket path: {}", server_soket_path.value().c_str());


    auto child_socket_path = get_child_socket_path();
    if (!child_socket_path)
    {
        spdlog::error("Cannot build child socket path");
        return EXIT_FAILURE;
    }
    spdlog::info("Child socket path: {}", child_socket_path.value().c_str());

    auto child_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (child_fd < 0)
    {
        spdlog::error("Cannot create child socket: {} {}", errno, strerror(errno));
        return EXIT_FAILURE;
    }
    FINALLY{ close(child_fd); };

    auto addr = sockaddr_un{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, child_socket_path.value().c_str(), sizeof(addr.sun_path));
    if (::bind(child_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
    {
        spdlog::error("Binding child fd to socket path {} failed: {} {}", child_socket_path.value().c_str(), errno, strerror(errno));
        return EXIT_FAILURE;
    }
    FINALLY{ ::unlink(child_socket_path.value().c_str()); };

    if (::listen(child_fd, socket_listen_queue_len) == -1)
    {
        spdlog::error("Marking child socket for listening failed: {} {}", errno, strerror(errno));
        return EXIT_FAILURE;
    }

    auto child_pid = ::fork();
    if (child_pid < 0)
    {
        spdlog::error("Fork failed: {} {}", errno, strerror(errno));
        return EXIT_FAILURE;
    }

    if (child_pid == 0)
    {
        // close(server_fd);
        // close(child_fd);
        setenv("WAYLAND_DISPLAY", child_socket_path.value().filename().c_str(), 1);

        std::vector<char*> child_argv;
        child_argv.reserve(static_cast<size_t>(argc));
        for (int i = 1; i < argc; ++i) {
            child_argv.push_back(argv[i]);
        }
        child_argv.push_back(nullptr);

        execvp(child_argv[0], child_argv.data());
        spdlog::error("execvp failed: {} {}", errno, strerror(errno));
        return EXIT_FAILURE;
    }

    spdlog::info("Child process ID: {}", child_pid);

    auto acceptor = std::jthread{[child_fd, socket_path = *server_soket_path](const std::stop_token& token)
    {
        auto child_threads = std::vector<std::jthread>{};
        while (!token.stop_requested())
        {
            auto pfd = pollfd{.fd = child_fd, .events = POLLIN, .revents = 0};
            auto ret = ::poll(&pfd, 1, acceptor_poll_timeout);
            if (ret == -1)
            {
                spdlog::info("Poll failed: {} {}", errno, strerror(errno));
            }
            if (ret == 0)
            {
                continue;
            }
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                spdlog::error("Error in polled fd: {:016b}", pfd.revents);
                break;
            }

            spdlog::info("Calling accept on {}", child_fd);
            auto conn_fd = ::accept(child_fd, nullptr, nullptr);
            if (conn_fd < 0)
            {
                spdlog::error("Accept failed: {} {}", errno, strerror(errno));
                continue;
            }
            spdlog::info("Accepted new connection: {}", conn_fd);
            child_threads.emplace_back([socket_path, conn_fd](const std::stop_token& token)
            {
                FINALLY{ close(conn_fd); };

                auto server_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
                if (server_fd < 0)
                {
                    spdlog::error("Cannot create server socket {}: {} {}", socket_path.c_str(), errno, strerror(errno));
                    return;
                }
                FINALLY{ close(server_fd); };

                auto srv_addr = sockaddr_un{};
                srv_addr.sun_family = AF_UNIX;
                std::strncpy(srv_addr.sun_path, socket_path.c_str(), sizeof(srv_addr.sun_path));
                if (connect(server_fd, reinterpret_cast<sockaddr*>(&srv_addr), sizeof(srv_addr)) == -1)
                {
                    spdlog::error("Connect to server failed: {} {}", errno, strerror(errno));
                    return;
                }

                spdlog::info("New worker thread running for {} {}", server_fd, conn_fd);

                run_loop(token, server_fd, conn_fd);
                spdlog::info("Worker thread for {} {} ended", server_fd, conn_fd);
            });
        }
        spdlog::info("Acceptor loop ended");
        for (auto& thread : child_threads)
        {
            spdlog::info("Requesting stop...");
            thread.request_stop();
        }
    }};

    int child_status = 0;
    auto wait_ret = ::waitpid(child_pid, &child_status, 0);
    spdlog::info("waitpid returned {}, status {} ({:032b})", wait_ret, child_status, child_status);
    acceptor.request_stop();
    spdlog::info("Requested stop, joining...");
    acceptor.join();
    if (WIFEXITED(child_status)) {
        return WEXITSTATUS(child_status);
    }
    if (WIFSIGNALED(child_status)) {
        return 128 + WTERMSIG(child_status);
    }

    return EXIT_SUCCESS;
}
