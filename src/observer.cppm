module;
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <poll.h>

export module observer;
import std;

using namespace std::literals;

enum class msg_kind
{
    request,
    event
};

namespace wire
{
    using uint_t = std::uint32_t;
    using int_t = std::int32_t;
    enum object_t: std::uint32_t{};
    using new_id_t = object_t;
}

template <> class fmt::formatter<wire::object_t> {
public:
    constexpr auto parse (format_parse_context& ctx) { return ctx.begin(); }
    template <typename Context>
    constexpr auto format (wire::object_t const& o, Context& ctx) const {
        auto prefix = o >= 0xFF000000 ? "sID:" : "cID:";
        auto val = o >= 0xFF000000 ? o - 0xFF000000 : o;
        return format_to(ctx.out(), "{}{}", prefix, val);
    }
};

struct protocol_base
{
    using new_ids = std::span<std::pair<wire::object_t, std::string_view>>;

    virtual std::string_view name() = 0;
    virtual std::string_view opcode_name(msg_kind kind, uint16_t opcode) = 0;
    virtual new_ids interpret(msg_kind kind, uint16_t opcode, std::span<std::byte> args) = 0;

    virtual ~protocol_base() = default;
};


namespace proto
{
    class wl_display: public protocol_base
    {
        std::vector<std::pair<wire::object_t, std::string_view>> new_ids_buf;

    public:
        std::string_view name() override
        {
            return "wl_display";
        }

        std::string_view opcode_name(msg_kind kind, uint16_t opcode) override
        {
            if (kind == msg_kind::request && opcode == 1)
            {
                return "get_registry";
            }
            return {};
        }

        new_ids interpret(msg_kind kind, uint16_t opcode, std::span<std::byte> args) override
        {
            if (kind == msg_kind::request && opcode == 1)
            {
                auto obj = *reinterpret_cast<wire::object_t*>(args.data());
                spdlog::info("    registry: {}:wl_registry", obj);
                new_ids_buf.clear();
                new_ids_buf.emplace_back(obj, "wl_registry");
                return new_ids_buf;
            }
            return {};
        }
    };
}

struct object_map
{
    std::unordered_map<wire::object_t, protocol_base*> object_to_interface;
    std::unordered_map<std::string_view, protocol_base*> name_to_interface;
    std::unordered_map<wire::object_t, std::string_view> object_to_iname;

    protocol_base* get_object_interface(wire::object_t obj)
    {
        auto iface = object_to_interface.find(obj);
        if (iface != object_to_interface.end())
        {
            return iface->second;
        }
        return nullptr;
    }

    std::string_view get_object_interface_name(wire::object_t obj)
    {
        auto iface = object_to_iname.find(obj);
        if (iface != object_to_iname.end())
        {
            return iface->second;
        }
        return {};
    }

    void register_object(wire::object_t obj, std::string_view interface)
    {
        if (auto [_, inserted] = object_to_iname.emplace(obj, interface); !inserted)
        {
            spdlog::error("Not registered: {} {}", obj, interface);
        }

        auto iface = name_to_interface.find(interface);
        if (iface == name_to_interface.end())
        {
            spdlog::warn("Registering {} {} is light: unknown interface", obj, interface);
            return;
        }

        if (auto [_, inserted] = object_to_interface.emplace(obj, iface->second); !inserted)
        {
            spdlog::error("Impl not registered: {} {}", obj, interface);
        }
    }

    proto::wl_display wl_display_impl = {};

    object_map()
    {
        name_to_interface[wl_display_impl.name()] = &wl_display_impl;
        object_to_interface[wire::object_t{1}] = &wl_display_impl;
        object_to_iname[wire::object_t{1}] = wl_display_impl.name();
    }
};


size_t inspect_message(std::span<std::byte> data, msg_kind kind, object_map& obj_map)
{
    auto cursor = data.data();

    // Message header
    // Word 1: object
    // Work 2: len | opcode
    const auto obj = *reinterpret_cast<wire::object_t*>(cursor);
    cursor += sizeof(wire::object_t);
    const auto len_opcode = *reinterpret_cast<std::uint32_t*>(cursor);
    const auto len = len_opcode >> 16;
    const auto opcode = len_opcode & 0x00FF;
    const auto kind_str = kind == msg_kind::request ? "[request]" : "[ event ]";

    auto obj_iface_name = obj_map.get_object_interface_name(obj);
    auto obj_impl = obj_map.get_object_interface(obj);

    if (obj_impl)
    {
        auto opcode_name = obj_impl->opcode_name(kind, opcode);
        if (!opcode_name.empty())
        {
            spdlog::info("{} {}:{} | len {} opcode {}:{} [left in packet {}]", kind_str, obj, obj_iface_name, len, opcode, opcode_name, data.size() - len);

            auto new_ids = obj_impl->interpret(kind, opcode, data.subspan(8, len - 8));
            for (auto& [obj, iname] : new_ids)
            {
                obj_map.register_object(obj, iname);
            }
        }
    }

    return len;
}

void inspect_packet(std::span<std::byte> data, msg_kind kind, object_map& obj_map)
{
    auto offset = 0zu;
    while (offset < data.size())
    {
        offset += inspect_message(data.subspan(offset), kind, obj_map);
    }
}

export void run_loop(const std::stop_token& stop, int server, int child)
{
    auto io_buf = std::array<std::byte, 16 * 1024>{};
    auto anc_buf = std::array<std::byte, 1024>{};
    auto obj_map = object_map{};

    auto forward_msg = [&io_buf, &anc_buf, &obj_map](int from, int to, msg_kind kind)
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
        inspect_packet({static_cast<std::byte*>(iov.iov_base), iov.iov_len}, kind, obj_map);

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
            spdlog::warn("poll {} {} returned {}, child {:016b}, server {:016b}", child, server, ret, fds[0].revents, fds[1].revents);
            return;
        }

        if (fds[0].revents & POLLIN)
        {
            forward_msg(child, server, msg_kind::request);
        }
        if (fds[1].revents & POLLIN)
        {
            forward_msg(server, child, msg_kind::event);
        }
    }
    spdlog::info("Stop flag is true");
}
