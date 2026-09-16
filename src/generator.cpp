#include <pugixml.hpp>
import std;
using namespace std::literals;

using dest_t = std::ostreambuf_iterator<char>;


void emit_begin_module(dest_t& dest, pugi::xml_node& node)
{
    constexpr auto text = "module;\n#include <spdlog/spdlog.h>\n"sv;
    constexpr auto text2 = "import std;\nimport interface_base;\nimport logging;\nusing namespace std::literals;\n\n"sv;

    std::copy(text.begin(), text.end(), dest);
    std::format_to(dest, "export module proto_{};\n", node.attribute("name").as_string());
    std::copy(text2.begin(), text2.end(), dest);
}

void emit_begin_protocol(dest_t& dest, pugi::xml_node& node)
{
    std::format_to(dest, "export namespace proto::{}\n{{\n", node.attribute("name").as_string());
}

void emit_begin_interface(dest_t dest, pugi::xml_node& node)
{
    std::format_to(dest, "    struct {} final : interface_base\n    {{\n", node.attribute("name").as_string());
}

void emit_request_event_enums(dest_t dest, pugi::xml_node& node)
{
    constexpr auto indent = "        ";
    std::format_to(dest, "{}enum request{{\n", indent);
    for (auto& ch: node.children())
    {
        if (ch.name() == "request"sv)
        {
            std::format_to(dest, "{}    {},\n", indent, ch.attribute("name").as_string());
        }
    }
    std::format_to(dest, "{}}};\n", indent);

    std::format_to(dest, "{}enum event{{\n", indent);
    for (auto& ch: node.children())
    {
        if (ch.name() == "event"sv)
        {
            std::format_to(dest, "{}    {},\n", indent, ch.attribute("name").as_string());
        }
    }
    std::format_to(dest, "{}}};\n", indent);
}

void emit_begin_silent_handlers(dest_t dest)
{
    constexpr auto text = "        static inline handler_table_t silent_impl = {\n"sv;
    std::copy(text.begin(), text.end(), dest);
}

void emit_begin_handlers_array(dest_t dest)
{
    constexpr auto text = "            handler_vector_t{\n"sv;
    std::copy(text.begin(), text.end(), dest);
}

void emit_silent_handler(dest_t dest, pugi::xml_node& node)
{
    constexpr auto indent = "                "sv;
    std::format_to(dest, "{}{}\n{}{{\n", indent, "[](wire::object_t obj, bytes_t args)", indent);

    for (auto& arg : node.children())
    {
        if (arg.name() == "arg"sv && arg.attribute("type").as_string() == "new_id"sv)
        {
            auto name = arg.attribute("name").as_string();
            if (arg.attribute("interface").empty())
            {
                std::format_to(dest, "{}    wire::string_t arg_{}_if;\n", indent, name);
                std::format_to(dest, "{}    wire::uint_t arg_{}_ver;\n", indent, name);
            }
            std::format_to(dest, "{}    wire::object_t arg_{};\n", indent, name);
        }
    }

    for (auto& arg : node.children())
    {
        if (arg.name() == "arg"sv)
        {
            auto arg_type = std::string_view(arg.attribute("type").as_string());
            if (arg_type == "new_id")
            {
                auto name = arg.attribute("name").as_string();
                if (arg.attribute("interface").empty())
                {
                    std::format_to(dest, "{}    std::tie(arg_{}_if, args) = read_string(args);\n", indent, name);
                    std::format_to(dest, "{}    std::tie(arg_{}_ver, args) = read_uint(args);\n", indent, name);
                }
                std::format_to(dest, "{}    std::tie(arg_{}, args) = read_object(args);\n", indent, name);
                if (arg.attribute("interface").empty())
                {
                    std::format_to(dest, "{}    on_new_object(arg_{}, arg_{}_if);\n", indent, name, name);
                }
                else
                {
                    std::format_to(dest, "{}    on_new_object(arg_{}, \"{}\");\n", indent, name, arg.attribute("interface").as_string());
                }
            }
            else
            {
                if (arg_type == "object" || arg_type == "uint" || arg_type == "int" || arg_type == "fixed")
                {
                    std::format_to(dest, "{}    args = skip_word(args);\n", indent);
                }
                else if (arg_type == "string" || arg_type == "array")
                {
                    std::format_to(dest, "{}    args = skip_multi_word(args);\n", indent);
                }
                else if (arg_type != "fd")
                {
                    std::println("Error: unexpected arg type: {}", arg_type);
                    exit(EXIT_FAILURE);
                }
            }
        }
    }

    if (node.attribute("type").as_string() == "destructor"sv)
    {
        std::format_to(dest, "{}    on_deleted_object(obj);\n", indent);
    }

    std::format_to(dest, "{}    return bytes_t{{}};\n", indent);
    std::format_to(dest, "{}}},\n", indent);
}

void emit_end_handlers_array(dest_t dest)
{
    constexpr auto text = "            },\n"sv;
    std::copy(text.begin(), text.end(), dest);
}

void emit_end_silent_handlers(dest_t dest)
{
    constexpr auto text = "        };\n"sv;
    std::copy(text.begin(), text.end(), dest);
}

void emit_begin_logging_handlers(dest_t dest)
{
    constexpr auto text = "        static inline handler_table_t logging_impl = {\n"sv;
    std::copy(text.begin(), text.end(), dest);
}

void emit_logging_handler(dest_t dest, pugi::xml_node& node)
{
    constexpr auto indent = "                "sv;
    std::format_to(dest, "{}{}\n{}{{\n", indent, "[](wire::object_t obj, bytes_t args)", indent);

    for (auto& arg : node.children())
    {
        if (arg.name() != "arg"sv)
        {
            continue;
        }

        auto name = arg.attribute("name").as_string();
        auto type_name = std::string_view{arg.attribute("type").as_string()};
        if (type_name == "new_id")
        {
            if (arg.attribute("interface").empty())
            {
                std::format_to(dest, "{}    wire::string_t arg_{}_if;\n", indent, name);
                std::format_to(dest, "{}    wire::uint_t arg_{}_ver;\n", indent, name);
            }
            std::format_to(dest, "{}    wire::object_t arg_{};\n", indent, name);
        }
        else if (type_name  != "fd")
        {
            std::format_to(dest, "{}    wire::{}_t arg_{};\n", indent, type_name, name);
        }
    }

    // arg_name, var_name, iface_suffix, runtime_iface
    auto processed_args = std::vector<std::tuple<std::string, std::string, std::string, std::string>>{};

    for (auto& arg : node.children())
    {
        if (arg.name() != "arg"sv)
        {
            continue;
        }

        auto name = arg.attribute("name").as_string();
        auto type_name = std::string_view{arg.attribute("type").as_string()};

        if (type_name == "new_id")
        {
            if (arg.attribute("interface").empty())
            {
                std::format_to(dest, "{}    std::tie(arg_{}_if, args) = read_string(args);\n", indent, name);
                std::format_to(dest, "{}    std::tie(arg_{}_ver, args) = read_uint(args);\n", indent, name);
            }
            std::format_to(dest, "{}    std::tie(arg_{}, args) = read_object(args);\n", indent, name);
            if (arg.attribute("interface").empty())
            {
                std::format_to(dest, "{}    on_new_object(arg_{}, arg_{}_if);\n", indent, name, name);
                processed_args.emplace_back(name, std::format("arg_{}", name), "<{}>", std::format("arg_{}_if", name));
            }
            else
            {
                std::format_to(dest, "{}    on_new_object(arg_{}, \"{}\");\n", indent, name, arg.attribute("interface").as_string());
                processed_args.emplace_back(name, std::format("arg_{}", name), std::format("<{}>", arg.attribute("interface").as_string()), "");
            }
        }
        else if (type_name  != "fd")
        {
            std::format_to(dest, "{}    std::tie(arg_{}, args) = read_{}(args);\n", indent, name, type_name);
            processed_args.emplace_back(name, std::format("arg_{}", name), "", "");
        }
        else
        {
            processed_args.emplace_back(name, "\"[fd]\"", "", "");
        }
    }

    // "[ req ] {}<wl_display>::get_registry(registry={}<wl_registry>)"

    auto kind = node.name() == "request"sv ? "[ req ]" : "[event]";
    std::format_to(dest, "{}    thread_logger().info(\"{} {{}}<{}>::{}(", indent, kind, node.parent().attribute("name").as_string(), node.attribute("name").as_string());
    bool first = true;
    for (auto& [arg_name, var_name, iface_suffix, runtime_iface]: processed_args)
    {
        if (first)
        {
            first = false;
        }
        else
        {
            std::format_to(dest, "{}", ",");
        }

        std::format_to(dest, "{}={{}}{}", arg_name, iface_suffix);
    }
    std::format_to(dest, "{}", ")\", obj");
    for (auto& [arg_name, var_name, iface_suffix, runtime_iface]: processed_args)
    {
        std::format_to(dest, ",{}", var_name);
        if (!runtime_iface.empty())
        {
            std::format_to(dest, ",{}", runtime_iface);
        }
    }
    std::format_to(dest, "{}", ");\n");

    if (node.attribute("type").as_string() == "destructor"sv)
    {
        std::format_to(dest, "{}    on_deleted_object(obj);\n", indent);
    }

    std::format_to(dest, "{}    return bytes_t{{}};\n", indent);
    std::format_to(dest, "{}}},\n", indent);
}

void emit_end_logging_handlers(dest_t dest)
{
    constexpr auto text = "        };\n"sv;
    std::copy(text.begin(), text.end(), dest);
}

void emit_end_interface(dest_t dest)
{
    constexpr auto text = "    };\n"sv;
    std::copy(text.begin(), text.end(), dest);
}

void emit_register_interfaces(dest_t dest, pugi::xml_node& node)
{
    constexpr auto decl = "    void register_wl_interfaces()\n    {\n"sv;
    std::copy(decl.begin(), decl.end(), dest);

    for (auto& ch: node.children())
    {
        if (ch.name() == "interface"sv)
        {
            auto name = ch.attribute("name").as_string();
            std::format_to(dest, "        interface_base::register_interface(\"{}\", {}::silent_impl, {}::logging_impl);\n", name, name, name);
        }
    }

    constexpr auto text = "    }\n"sv;
    std::copy(text.begin(), text.end(), dest);
}

void emit_end_protocol_module(dest_t dest)
{
    constexpr auto text = "}\n"sv;
    std::copy(text.begin(), text.end(), dest);
}


int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::println("2 arguments required");
        return EXIT_FAILURE;
    }

    auto input = argv[1];
    auto output = argv[2];

    pugi::xml_document doc;
    auto parsed = doc.load_file(input);
    if (!parsed)
    {
        std::println("Parsing failed: {}", parsed.description());
        return EXIT_FAILURE;
    }

    auto dest_file = std::ofstream(output);
    if (!dest_file)
    {
        std::println("Failed to open destination file: {}", output);
        return EXIT_FAILURE;
    }
    auto dest = dest_t{dest_file};
    auto root = doc.child("protocol");

    emit_begin_module(dest, root);
    emit_begin_protocol(dest, root);

    for (auto& ch: root.children())
    {
        if (ch.name() == "copyright"sv)
        {
            continue;
        }

        emit_begin_interface(dest, ch);
        emit_request_event_enums(dest, ch);
        emit_begin_silent_handlers(dest);
        emit_begin_handlers_array(dest);
        for (auto& item: ch.children())
        {
            if (item.name() == "request"sv)
            {
                emit_silent_handler(dest, item);
            }
        }
        emit_end_handlers_array(dest);
        emit_begin_handlers_array(dest);
        for (auto& item: ch.children())
        {
            if (item.name() == "event"sv)
            {
                emit_silent_handler(dest, item);
            }
        }
        emit_end_handlers_array(dest);
        emit_end_silent_handlers(dest);

        emit_begin_logging_handlers(dest);
        emit_begin_handlers_array(dest);
        for (auto& item: ch.children())
        {
            if (item.name() == "request"sv)
            {
                emit_logging_handler(dest, item);
            }
        }
        emit_end_handlers_array(dest);
        emit_begin_handlers_array(dest);
        for (auto& item: ch.children())
        {
            if (item.name() == "event"sv)
            {
                emit_logging_handler(dest, item);
            }
        }
        emit_end_handlers_array(dest);
        emit_end_logging_handlers(dest);
        emit_end_interface(dest);
    }

    emit_register_interfaces(dest, root);
    emit_end_protocol_module(dest);

    return EXIT_SUCCESS;
}
