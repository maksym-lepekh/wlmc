#include <pugixml.hpp>
import std;
using namespace std::literals;

using dest_t = std::ostreambuf_iterator<char>;


void emit_begin_module(dest_t& dest, pugi::xml_node& node)
{
    constexpr auto text = "module;\n#include <spdlog/spdlog.h>\n"sv;
    constexpr auto text2 = "import std;\nimport interface_base;\nusing namespace std::literals;\n\n"sv;

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
    std::format_to(dest, "{}{}\n{}{{\n", indent, "[](wire::object_t, std::span<std::byte> args)", indent);

    for (auto& arg : node.children())
    {
        if (arg.name() == "arg"sv && arg.attribute("type").as_string() == "new_id"sv)
        {
            std::format_to(dest, "{}    wire::object_t arg_{};\n", indent, arg.attribute("name").as_string());
        }
    }

    for (auto& arg : node.children())
    {
        if (arg.name() == "arg"sv)
        {
            auto arg_type = std::string_view(arg.attribute("type").as_string());
            if (arg_type == "new_id")
            {
                std::format_to(dest, "{}    std::tie(arg_{}, args) = read_object_id(args);\n", indent, arg.attribute("name").as_string());
                std::format_to(dest, "{}    on_new_object(arg_{}, \"{}\");\n", indent, arg.attribute("name").as_string(), arg.attribute("interface").as_string());
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
    // todo
    std::format_to(dest, "                {},\n", "noop_handler");
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
