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
import interface_base;
import intercept;

import proto_wayland;
import proto_linux_dmabuf_v1;
import proto_xdg_shell;
import proto_linux_drm_syncobj_v1;
import proto_presentation_time;
import proto_color_management_v1;
import proto_commit_timing_v1;
import proto_fifo_v1;
import proto_viewporter;
import proto_fractional_scale_v1;
import proto_xdg_output_unstable_v1;
import proto_pointer_warp_v1;
import proto_pointer_constraints_unstable_v1;
import proto_cursor_shape_v1;
import proto_xdg_toplevel_tag_v1;
import proto_xdg_activation_v1;
import proto_keyboard_shortcuts_inhibit_unstable_v1;
import proto_relative_pointer_unstable_v1;

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

interface_base::bytes_t destroy_registry(wire::object_t, interface_base::bytes_t args)
{
    wire::object_t arg_registry;
    std::tie(arg_registry, args) = interface_base::read_object(args);
    interface_base::on_deleted_object(arg_registry);
    return {};
}

void set_silent(const char* interface, wire::msg_kind kind, size_t opcode)
{
    interface_base::logging_map[interface][kind][opcode] = interface_base::silent_map[interface][kind][opcode];
}

void register_wayland_interfaces()
{
    proto::wayland::register_wl_interfaces();
    proto::linux_dmabuf_v1::register_wl_interfaces();
    proto::xdg_shell::register_wl_interfaces();
    proto::linux_drm_syncobj_v1::register_wl_interfaces();
    proto::presentation_time::register_wl_interfaces();
    proto::color_management_v1::register_wl_interfaces();
    proto::commit_timing_v1::register_wl_interfaces();
    proto::fifo_v1::register_wl_interfaces();
    proto::viewporter::register_wl_interfaces();
    proto::fractional_scale_v1::register_wl_interfaces();
    proto::xdg_output_unstable_v1::register_wl_interfaces();
    proto::pointer_warp_v1::register_wl_interfaces();
    proto::pointer_constraints_unstable_v1::register_wl_interfaces();
    proto::cursor_shape_v1::register_wl_interfaces();
    proto::xdg_toplevel_tag_v1::register_wl_interfaces();
    proto::xdg_activation_v1::register_wl_interfaces();
    proto::keyboard_shortcuts_inhibit_unstable_v1::register_wl_interfaces();
    proto::relative_pointer_unstable_v1::register_wl_interfaces();

    interface_base::silent_map["wl_fixes"][wire::request][proto::wayland::wl_fixes::request::destroy_registry] = destroy_registry;
    interface_base::logging_map["wl_fixes"][wire::request][proto::wayland::wl_fixes::request::destroy_registry] = destroy_registry;

    // set_silent("wl_surface", wire::request, proto::wayland::wl_surface::request::commit);
    // set_silent("wl_surface", wire::request, proto::wayland::wl_surface::request::damage_buffer);
    // set_silent("wl_surface", wire::request, proto::wayland::wl_surface::request::attach);

    // set_silent("wl_surface", wire::request, proto::wayland::wl_surface::request::set_input_region);
    set_silent("wl_surface", wire::request, proto::wayland::wl_surface::request::damage_buffer);
    // set_silent("wl_surface", wire::request, proto::wayland::wl_surface::request::attach);
    set_silent("wl_surface", wire::request, proto::wayland::wl_surface::request::commit);
    // set_silent("wl_pointer", wire::event, proto::wayland::wl_pointer::event::frame);
    set_silent("xdg_wm_base", wire::event, proto::xdg_shell::xdg_wm_base::event::ping);
    set_silent("xdg_wm_base", wire::request, proto::xdg_shell::xdg_wm_base::request::pong);
    // set_silent("wp_viewport", wire::request, proto::viewporter::wp_viewport::request::set_source);
    // set_silent("wl_buffer", wire::event, proto::wayland::wl_buffer::event::release);


    // set_silent("xdg_surface", wire::request, proto::xdg_shell::xdg_surface::request::set_window_geometry);
    // set_silent("xdg_toplevel", wire::request, proto::xdg_shell::xdg_toplevel::request::set_min_size);
    // set_silent("xdg_toplevel", wire::request, proto::xdg_shell::xdg_toplevel::request::set_max_size);


    auto& impl_map = interface_base::silent_map;
    impl_map["wp_fractional_scale_v1"][wire::event][proto::fractional_scale_v1::wp_fractional_scale_v1::event::preferred_scale] = fractional_scale_preferred_scale;
    impl_map["wl_output"][wire::event][proto::wayland::wl_output::event::mode] = wl_output_mode;
    impl_map["wl_output"][wire::event][proto::wayland::wl_output::event::scale] = wl_output_scale;

    impl_map["xdg_toplevel"][wire::event][proto::xdg_shell::xdg_toplevel::event::configure] = toplevel_configure;
    impl_map["wp_viewport"][wire::request][proto::viewporter::wp_viewport::request::set_destination] = viewport_set_destination;
    impl_map["xdg_surface"][wire::request][proto::xdg_shell::xdg_surface::request::set_window_geometry] = xdg_set_window_geometry;
    impl_map["xdg_toplevel"][wire::request][proto::xdg_shell::xdg_toplevel::request::set_min_size] = xdg_set_min_size;
    impl_map["xdg_toplevel"][wire::request][proto::xdg_shell::xdg_toplevel::request::set_max_size] = xdg_set_max_size;

    impl_map["wl_region"][wire::request][proto::wayland::wl_region::request::add] = region_add;

    impl_map["wl_pointer"][wire::event][proto::wayland::wl_pointer::event::motion] = pointer_motion;
    impl_map["xdg_toplevel"][wire::request][proto::xdg_shell::xdg_toplevel::request::set_fullscreen] = xdg_set_fullscreen;
    impl_map["xdg_toplevel"][wire::request][proto::xdg_shell::xdg_toplevel::request::unset_fullscreen] = xdg_unset_fullscreen;
    impl_map["zxdg_output_v1"][wire::event][proto::xdg_output_unstable_v1::zxdg_output_v1::event::logical_size] = logical_size;

    impl_map["zwp_locked_pointer_v1"][wire::request][proto::pointer_constraints_unstable_v1::zwp_locked_pointer_v1::request::set_cursor_position_hint] = set_cursor_position_hint;
}

int main(int argc, char** argv)
{
    spdlog::set_level(spdlog::level::warn);

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

    register_wayland_interfaces();
    auto acceptor = std::jthread{[child_fd, socket_path = *server_soket_path](const std::stop_token& token)
    {
        auto child_threads = std::vector<std::jthread>{};
        while (!token.stop_requested())
        {
            auto pfd = pollfd{.fd = child_fd, .events = POLLIN, .revents = 0};
            auto ret = ::poll(&pfd, 1, acceptor_poll_timeout);
            if (ret == -1)
            {
                spdlog::warn("Poll failed: {} {}", errno, strerror(errno));
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
