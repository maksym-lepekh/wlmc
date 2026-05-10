module;
#include <spdlog/spdlog.h>

export module core_wayland;
import std;
import interface_base;
using namespace std::literals;

export namespace proto::wayland
{
    struct wl_display final : interface_base
    {
        static inline std::array error_enum_strings = {
            "invalid_object",
            "invalid_method",
            "no_memory",
        };

        static inline handler_table_t silent_impl = {
            handler_vector_t{
                [](wire::object_t, std::span<std::byte> args)
                {
                    wire::object_t arg_callback;
                    std::tie(arg_callback, args) = read_object_id(args);
                    on_new_object(arg_callback, "wl_callback");
                },
                [](wire::object_t, std::span<std::byte> args)
                {
                    wire::object_t arg_registry;
                    std::tie(arg_registry, args) = read_object_id(args);
                    on_new_object(arg_registry, "wl_registry");
                }
            },
            handler_vector_t{
                noop_handler,
                noop_handler,
                noop_handler,
                noop_handler
            },
        };

        static inline handler_table_t logging_impl = {
            handler_vector_t{
                [](wire::object_t instance, std::span<std::byte> args)
                {
                    wire::object_t arg_callback;
                    std::tie(arg_callback, args) = read_object_id(args);
                    on_new_object(arg_callback, "wl_callback");
                    spdlog::info("[ req ] {}<wl_display>::sync(callback={}<wl_callback>)", instance, arg_callback);
                },
                [](wire::object_t instance, std::span<std::byte> args)
                {
                    wire::object_t arg_registry;
                    std::tie(arg_registry, args) = read_object_id(args);
                    on_new_object(arg_registry, "wl_registry");
                    spdlog::info("[ req ] {}<wl_display>::get_registry(registry={}<wl_registry>)", instance, arg_registry);
                }
            }, handler_vector_t{
                noop_handler,
                noop_handler,
                noop_handler,
                noop_handler,
            },
        };
    };

    void register_wl_interfaces()
    {
        interface_base::register_interface("wl_display", wl_display::silent_impl, wl_display::logging_impl);
    }
}
