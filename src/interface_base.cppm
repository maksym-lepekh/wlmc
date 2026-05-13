module;
#include <spdlog/spdlog.h>

export module interface_base;
import std;

export namespace wire
{
    using uint_t = std::uint32_t;
    using int_t = std::int32_t;
    enum object_t: std::uint32_t{};
    using new_id_t = object_t;
    using string_t = std::string_view;
    using array_t = std::span<std::byte>;
    using fixed_t = std::uint32_t;

    enum msg_kind
    {
        request,
        event
    };
}

template <> struct fmt::formatter<wire::object_t>
{
    static constexpr auto parse (const format_parse_context& ctx)
    {
        return ctx.begin();
    }

    template <typename Context>
    constexpr auto format (wire::object_t const& o, Context& ctx) const
    {
        auto prefix = o >= 0xFF000000 ? "sID:" : "cID:";
        auto val = o >= 0xFF000000 ? o - 0xFF000000 : o;
        return format_to(ctx.out(), "{}{}", prefix, val);
    }
};

template <> struct fmt::formatter<wire::array_t>
{
    static constexpr auto parse (const format_parse_context& ctx)
    {
        return ctx.begin();
    }

    template <typename Context>
    constexpr auto format (wire::array_t const& a, Context& ctx) const
    {
        return format_to(ctx.out(), "[{} bytes]", a.size());
    }
};

export class interface_base
{
public:
    using bytes_t = std::span<std::byte>;
    using message_handler_t = void(*)(wire::object_t, bytes_t);
    using handler_vector_t = std::inplace_vector<message_handler_t, 32>;
    using handler_table_t = std::array<handler_vector_t, 2>;

    static void dispatch(wire::object_t obj, wire::msg_kind kind, uint16_t opcode, bytes_t args);
    static void register_interface(std::string_view name, const handler_table_t& silent_vtable, const handler_table_t& logging_vtable);
    static void on_new_object(wire::object_t obj, std::string_view interface_name);
    static void on_deleted_object(wire::object_t obj);

    static inline auto noop_handler = [](wire::object_t, bytes_t){};

    static bytes_t skip_word(bytes_t bytes);
    static bytes_t skip_multi_word(bytes_t bytes);
    static std::pair<wire::uint_t, bytes_t> read_uint(bytes_t bytes);
    static std::pair<wire::int_t, bytes_t> read_int(bytes_t bytes);
    static std::pair<wire::string_t, bytes_t> read_string(bytes_t bytes);
    static std::pair<wire::array_t, bytes_t> read_array(bytes_t bytes);
    static std::pair<wire::object_t, bytes_t> read_object(bytes_t bytes);
    static std::pair<wire::fixed_t, bytes_t> read_fixed(bytes_t bytes);

public:
    static inline std::unordered_map<std::string_view, handler_table_t> silent_map = {};
    static inline std::unordered_map<std::string_view, handler_table_t> logging_map = {};

private:
    static inline thread_local std::unordered_map<wire::object_t, std::string> known_objects = {};
};


void interface_base::dispatch(wire::object_t obj, wire::msg_kind kind, uint16_t opcode, bytes_t args)
{
    auto found = known_objects.find(obj);
    if (found == known_objects.end())
    {
        spdlog::debug("Unknown object: {}", obj);
        return;
    }

    auto impl = logging_map.find(found->second);
    if (impl != logging_map.end())
    {
        if (opcode >= impl->second[kind].size())
        {
            spdlog::error("Opcode {} out of range of {}'s vtable. Object: {}", opcode, impl->first, obj);
            return;
        }
        impl->second[kind][opcode](obj, args);
    }
    else
    {
        spdlog::debug("No interface impl for {}<{}>", obj, found->second);
    }
}

void interface_base::register_interface(const std::string_view name, const handler_table_t& silent_vtable, const handler_table_t& logging_vtable)
{
    silent_map[name] = silent_vtable;
    logging_map[name] = logging_vtable;
}

void interface_base::on_new_object(wire::object_t obj, std::string_view interface_name)
{
    spdlog::debug("on_new_object: {} {}", obj, interface_name);
    known_objects[obj] = interface_name;
}

void interface_base::on_deleted_object(wire::object_t obj)
{
    spdlog::debug("on_deleted_object: {}", obj);
    known_objects.erase(obj);
}

interface_base::bytes_t interface_base::skip_word(bytes_t bytes)
{
    return bytes.subspan(4);
}

interface_base::bytes_t interface_base::skip_multi_word(bytes_t bytes)
{
    auto len = *reinterpret_cast<std::uint32_t*>(bytes.data());
    auto padded = 4 * (len / 4) + (len % 4 ? 4 : 0);
    return bytes.subspan(4 + padded);
}

std::pair<wire::uint_t, interface_base::bytes_t> interface_base::read_uint(bytes_t bytes)
{
    auto val = *reinterpret_cast<std::uint32_t*>(bytes.data());
    return {val, bytes.subspan(4)};
}

std::pair<wire::int_t, interface_base::bytes_t> interface_base::read_int(bytes_t bytes)
{
    auto val = *reinterpret_cast<std::int32_t*>(bytes.data());
    return {val, bytes.subspan(4)};
}

std::pair<wire::string_t, interface_base::bytes_t> interface_base::read_string(bytes_t bytes)
{
    auto len = *reinterpret_cast<std::uint32_t*>(bytes.data()) - 1;
    auto padded = 4 * (len / 4) + (len % 4 ? 4 : 0);
    return {std::string_view{reinterpret_cast<const char*>(bytes.data() + 4), len}, bytes.subspan(4 + padded)};
}

std::pair<wire::array_t, interface_base::bytes_t> interface_base::read_array(bytes_t bytes)
{
    auto len = *reinterpret_cast<std::uint32_t*>(bytes.data());
    auto padded = 4 * (len / 4) + (len % 4 ? 4 : 0);
    return {bytes.subspan(4, len), bytes.subspan(4 + padded)};
}

std::pair<wire::object_t, interface_base::bytes_t> interface_base::read_object(bytes_t bytes)
{
    auto val = *reinterpret_cast<wire::object_t*>(bytes.data());
    return {val, bytes.subspan(sizeof(val))};
}

std::pair<wire::fixed_t, interface_base::bytes_t> interface_base::read_fixed(bytes_t bytes)
{
    // todo impl real type
    auto val = *reinterpret_cast<wire::fixed_t*>(bytes.data());
    return {val, bytes.subspan(4)};
}
