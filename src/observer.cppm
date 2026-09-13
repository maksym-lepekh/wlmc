module;
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <poll.h>

export module observer;
import std;
import interface_base;

using namespace std::literals;


namespace
{
    size_t inspect_message(std::span<std::byte> data, wire::msg_kind kind)
    {
        auto cursor = data.data();

        const auto obj = *reinterpret_cast<wire::object_t*>(cursor);
        cursor += sizeof(wire::object_t);

        const auto len_opcode = *reinterpret_cast<std::uint32_t*>(cursor);
        const auto len = len_opcode >> 16;
        const auto opcode = len_opcode & 0x00FF;
        interface_base::dispatch(obj, kind, opcode, data.subspan(8, len - 8));
        return len;
    }

    void inspect_packet(std::span<std::byte> data, wire::msg_kind kind)
    {
        auto offset = 0zu;
        while (offset < data.size())
        {
            offset += inspect_message(data.subspan(offset), kind);
        }
    }
}

export void run_loop(const std::stop_token& stop, int server, int child)
{
    interface_base::on_new_object(wire::object_t{1}, "wl_display");

    auto io_buf = std::array<std::byte, 16 * 1024>{};
    auto anc_buf = std::array<std::byte, 1024>{};

    auto forward_msg = [&io_buf, &anc_buf](int from, int to, wire::msg_kind kind)
    {
        auto socket_msg = msghdr{};
        auto iov = iovec{.iov_base = io_buf.data(), .iov_len = io_buf.size()};
        socket_msg.msg_iov = &iov;
        socket_msg.msg_iovlen = 1;
        socket_msg.msg_control = anc_buf.data();
        socket_msg.msg_controllen = anc_buf.size();

        auto bytes = ::recvmsg(from, &socket_msg, 0);
        if (bytes < 0)
        {
            spdlog::error("recvmsg failed {} {}", errno, strerror(errno));
            return;
        }
        spdlog::trace("Read from {}: {} bytes", from, bytes);
        if (bytes % 4 != 0)
        {
            spdlog::warn("Non-32 bit message");
        }

        iov.iov_len = bytes;
        inspect_packet({static_cast<std::byte*>(iov.iov_base), iov.iov_len}, kind);

        bytes = sendmsg(to, &socket_msg, MSG_NOSIGNAL);
        if (bytes < 0)
        {
            spdlog::error("sendmsg failed {} {}", errno, strerror(errno));
            return;
        }
        spdlog::trace("Send to {}: {} bytes", to, bytes);
    };

    while (!stop.stop_requested())
    {
        auto fds = std::array<pollfd, 2>{};
        fds[0].fd = child;
        fds[0].events = POLLIN;
        fds[1].fd = server;
        fds[1].events = POLLIN;
        auto ret = poll(fds.data(), fds.size(), -1);
        if (ret < 0)
        {
            spdlog::error("poll failed {} {}", errno, strerror(errno));
            if (errno != EINTR)
            {
                return;
            }
            continue;
        }

        if (ret == 0)
        {
            continue;
        }

        if ((fds[0].revents | fds[1].revents) & (POLLHUP | POLLERR | POLLNVAL))
        {
            spdlog::info("poll {} {} returned {}, child {:016b}, server {:016b}", child, server, ret, fds[0].revents, fds[1].revents);
            return;
        }

        if (fds[0].revents & POLLIN)
        {
            forward_msg(child, server, wire::msg_kind::request);
        }
        if (fds[1].revents & POLLIN)
        {
            forward_msg(server, child, wire::msg_kind::event);
        }
    }
    spdlog::info("Stop flag is true");
}
