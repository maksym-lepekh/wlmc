module;
#include <spdlog/spdlog.h>

export module intercept;

import std;
import interface_base;
import logging;

using bytes_t = interface_base::bytes_t;
using ib = interface_base;

size_t write_uint(std::byte* buf, wire::uint_t val)
{
    *reinterpret_cast<wire::uint_t*>(buf) = val;
    return sizeof(wire::uint_t);
}

size_t write_int(std::byte* buf, wire::int_t val)
{
    *reinterpret_cast<wire::int_t*>(buf) = val;
    return sizeof(wire::int_t);
}

size_t write_fixed(std::byte* buf, wire::fixed_t val)
{
    *reinterpret_cast<decltype(val.fixed_repr)*>(buf) = val.fixed_repr;
    return sizeof(decltype(val.fixed_repr));
}

namespace
{
    thread_local std::array<std::byte, 16 * 1024> buf{};

    thread_local wire::int_t host_width = 0;
    thread_local wire::int_t host_height = 0;
    thread_local wire::int_t host_scale = 0;
    thread_local wire::uint_t host_frac_scale = 0;
    thread_local wire::int_t host_scaled_width = 0;
    thread_local wire::int_t host_scaled_height = 0;

    thread_local wire::int_t half_width = 0;
    thread_local wire::int_t half_height = 0;

    thread_local bool is_fullscreen = false;
}

export bytes_t fractional_scale_preferred_scale(wire::object_t obj, interface_base::bytes_t args)
{
    assert(args.size() < 1024);
    std::memcpy(buf.data(), args.data(), args.size());
    auto args_orig = args;

    wire::uint_t arg_scale;
    std::tie(arg_scale, args) = ib::read_uint(args);
    thread_logger().debug("[ !!! ] {}<wp_fractional_scale_v1>::preferred_scale(scale={})", obj,arg_scale);

    host_frac_scale = arg_scale;
    write_uint(buf.data(), 120);
    thread_logger().debug("[event] {}<wp_fractional_scale_v1>::preferred_scale(scale={})", obj, 120);
    return {buf.data(), args_orig.size()};
}

export bytes_t wl_output_mode(wire::object_t obj, bytes_t args)
{
    assert(args.size() < 1024);
    std::memcpy(buf.data(), args.data(), args.size());
    auto args_orig = args;

    wire::uint_t arg_flags;
    wire::int_t arg_width;
    wire::int_t arg_height;
    wire::int_t arg_refresh;
    std::tie(arg_flags, args) = ib::read_uint(args);
    auto width_pos = args.data() - args_orig.data();
    std::tie(arg_width, args) = ib::read_int(args);
    auto heigth_pos = args.data() - args_orig.data();
    std::tie(arg_height, args) = ib::read_int(args);
    std::tie(arg_refresh, args) = ib::read_int(args);
    thread_logger().debug("[!   !] {}<wl_output>::mode(flags={},width={},height={},refresh={})", obj,arg_flags,arg_width,arg_height,arg_refresh);

    host_width = arg_width;
    host_height = arg_height;
    half_width = host_width / 2;
    half_height = host_height / 2;
    write_int(buf.data() + width_pos, half_width);
    write_int(buf.data() + heigth_pos, half_height);
    thread_logger().debug("[event] {}<wl_output>::mode(flags={},width={},height={},refresh={})", obj,arg_flags,arg_width / 2,arg_height / 2,arg_refresh);
    return {buf.data(), args_orig.size()};
}

export bytes_t wl_output_scale(wire::object_t obj, bytes_t args)
{
    assert(args.size() < 1024);
    std::memcpy(buf.data(), args.data(), args.size());
    auto args_orig = args;

    wire::int_t arg_factor;
    std::tie(arg_factor, args) = ib::read_int(args);
    thread_logger().debug("[!   !] {}<wl_output>::scale(factor={})", obj,arg_factor);

    host_scale = arg_factor;
    write_int(buf.data(), 1);
    thread_logger().debug("[event] {}<wl_output>::scale(factor={})", obj,1);
    return {buf.data(), args_orig.size()};
}

export bytes_t toplevel_configure(wire::object_t obj, bytes_t args)
{
    assert(args.size() < 1024);
    std::memcpy(buf.data(), args.data(), args.size());
    auto args_orig = args;

    wire::int_t arg_width;
    wire::int_t arg_height;
    wire::array_t arg_states;
    auto width_pos = args.data() - args_orig.data();
    std::tie(arg_width, args) = ib::read_int(args);
    auto heigth_pos = args.data() - args_orig.data();
    std::tie(arg_height, args) = ib::read_int(args);
    std::tie(arg_states, args) = ib::read_array(args);
    // thread_logger().debug("[!   !] {}<xdg_toplevel>::configure(width={},height={},states={})", obj,arg_width,arg_height,arg_states);

    if (arg_width == 0 || arg_height == 0)
    {
        return {};
    }

    host_scaled_width = arg_width;
    host_scaled_height = arg_height;
    // write_int(buf.data() + width_pos, half_width);
    // write_int(buf.data() + heigth_pos, half_height);
    // thread_logger().debug("[event] {}<xdg_toplevel>::configure(width={},height={},states={})", obj,half_width,half_height,arg_states);
    // return {buf.data(), args_orig.size()};
    return {};
}

export bytes_t viewport_set_destination(wire::object_t obj, bytes_t args)
{
    assert(args.size() < 1024);
    std::memcpy(buf.data(), args.data(), args.size());
    auto args_orig = args;

    wire::int_t arg_width;
    wire::int_t arg_height;
    auto width_pos = args.data() - args_orig.data();
    std::tie(arg_width, args) = ib::read_int(args);
    auto heigth_pos = args.data() - args_orig.data();
    std::tie(arg_height, args) = ib::read_int(args);
    thread_logger().debug("[!req!] {}<wp_viewport>::set_destination(width={},height={})", obj,arg_width,arg_height);

    if (arg_width != half_width || arg_height != half_height)
    {
        return bytes_t{};
    }

    write_int(buf.data() + width_pos, host_scaled_width);
    write_int(buf.data() + heigth_pos, host_scaled_height);
    thread_logger().debug("[ req ] {}<wp_viewport>::set_destination(width={},height={})", obj,host_scaled_width,host_scaled_height);
    return {buf.data(), args_orig.size()};
}

export bytes_t xdg_set_window_geometry(wire::object_t obj, bytes_t args)
{
    assert(args.size() < 1024);
    std::memcpy(buf.data(), args.data(), args.size());
    auto args_orig = args;

    wire::int_t arg_x;
    wire::int_t arg_y;
    wire::int_t arg_width;
    wire::int_t arg_height;
    std::tie(arg_x, args) = ib::read_int(args);
    std::tie(arg_y, args) = ib::read_int(args);
    auto width_pos = args.data() - args_orig.data();
    std::tie(arg_width, args) = ib::read_int(args);
    auto heigth_pos = args.data() - args_orig.data();
    std::tie(arg_height, args) = ib::read_int(args);
    thread_logger().debug("[!req!] {}<xdg_surface>::set_window_geometry(x={},y={},width={},height={})", obj,arg_x,arg_y,arg_width,arg_height);

    if (arg_width == half_width && arg_height == half_height)
    {
        write_int(buf.data() + width_pos, host_scaled_width);
        write_int(buf.data() + heigth_pos, host_scaled_height);
        thread_logger().debug("[ req ] {}<xdg_surface>::set_window_geometry(x={},y={},width={},height={})", obj,arg_x,arg_y,host_scaled_width,host_scaled_height);
        return {buf.data(), args_orig.size()};
    }
    return bytes_t{};
}

export bytes_t xdg_set_min_size(wire::object_t obj, bytes_t args)
{
    assert(args.size() < 1024);
    std::memcpy(buf.data(), args.data(), args.size());
    auto args_orig = args;

    wire::int_t arg_width;
    wire::int_t arg_height;
    auto width_pos = args.data() - args_orig.data();
    std::tie(arg_width, args) = ib::read_int(args);
    auto heigth_pos = args.data() - args_orig.data();
    std::tie(arg_height, args) = ib::read_int(args);
    thread_logger().debug("[!req!] {}<xdg_toplevel>::set_min_size(width={},height={})", obj,arg_width,arg_height);

    if (arg_width == half_width && arg_height == half_height)
    {
        write_int(buf.data() + width_pos, host_scaled_width);
        write_int(buf.data() + heigth_pos, host_scaled_height);
        thread_logger().debug("[ req ] {}<xdg_toplevel>::set_min_size(width={},height={})", obj,host_scaled_width, host_scaled_height);
        return {buf.data(), args_orig.size()};
    }

    return bytes_t{};
}

export bytes_t xdg_set_max_size(wire::object_t obj, bytes_t args)
{
    assert(args.size() < 1024);
    std::memcpy(buf.data(), args.data(), args.size());
    auto args_orig = args;

    wire::int_t arg_width;
    wire::int_t arg_height;
    auto width_pos = args.data() - args_orig.data();
    std::tie(arg_width, args) = ib::read_int(args);
    auto heigth_pos = args.data() - args_orig.data();
    std::tie(arg_height, args) = ib::read_int(args);
    thread_logger().debug("[!req!] {}<xdg_toplevel>::set_max_size(width={},height={})", obj,arg_width,arg_height);

    if (arg_width == half_width && arg_height == half_height)
    {
        write_int(buf.data() + width_pos, host_scaled_width);
        write_int(buf.data() + heigth_pos, host_scaled_height);
        thread_logger().debug("[ req ] {}<xdg_toplevel>::set_max_size(width={},height={})", obj,host_scaled_width, host_scaled_height);
        return {buf.data(), args_orig.size()};
    }

    return bytes_t{};
}

export bytes_t region_add(wire::object_t obj, bytes_t args)
{
    assert(args.size() < 1024);
    std::memcpy(buf.data(), args.data(), args.size());
    auto args_orig = args;

    wire::int_t arg_x;
    wire::int_t arg_y;
    wire::int_t arg_width;
    wire::int_t arg_height;
    std::tie(arg_x, args) = ib::read_int(args);
    std::tie(arg_y, args) = ib::read_int(args);
    auto width_pos = args.data() - args_orig.data();
    std::tie(arg_width, args) = ib::read_int(args);
    auto heigth_pos = args.data() - args_orig.data();
    std::tie(arg_height, args) = ib::read_int(args);
    thread_logger().debug("[!req!] {}<wl_region>::add(x={},y={},width={},height={})", obj,arg_x,arg_y,arg_width,arg_height);

    if (arg_width == half_width && arg_height == half_height)
    {
        write_int(buf.data() + width_pos, host_scaled_width);
        write_int(buf.data() + heigth_pos, host_scaled_height);
        thread_logger().debug("[ req ] {}<wl_region>::add(x={},y={},width={},height={})", obj,arg_x,arg_y,host_scaled_width, host_scaled_height);
        return {buf.data(), args_orig.size()};
    }

    return bytes_t{};
}

export bytes_t pointer_motion(wire::object_t obj, bytes_t args)
{
    assert(args.size() < 1024);
    std::memcpy(buf.data(), args.data(), args.size());
    auto args_orig = args;

    wire::uint_t arg_time;
    wire::fixed_t arg_surface_x;
    wire::fixed_t arg_surface_y;
    std::tie(arg_time, args) = ib::read_uint(args);
    auto x_pos = args.data() - args_orig.data();
    std::tie(arg_surface_x, args) = ib::read_fixed(args);
    auto y_pos = args.data() - args_orig.data();
    std::tie(arg_surface_y, args) = ib::read_fixed(args);
    thread_logger().debug("[! ev !] {}<wl_pointer>::motion(time={},surface_x={},surface_y={})", obj,arg_time,arg_surface_x,arg_surface_y);

    if (is_fullscreen)
    {
        double ratio = (double)half_width / (double)host_scaled_width;
        arg_surface_x = wire::fixed_t{arg_surface_x.double_repr * ratio};
        arg_surface_y = wire::fixed_t{arg_surface_y.double_repr * ratio};
        write_fixed(buf.data() + x_pos, arg_surface_x);
        write_fixed(buf.data() + y_pos, arg_surface_y);
        thread_logger().debug("[event] {}<wl_pointer>::motion(time={},surface_x={},surface_y={})", obj,arg_time,arg_surface_x,arg_surface_y);
        return {buf.data(), args_orig.size()};
    }

    return bytes_t{};
}

export bytes_t xdg_set_fullscreen(wire::object_t obj, bytes_t args)
{
    wire::object_t arg_output;
    std::tie(arg_output, args) = ib::read_object(args);
    thread_logger().debug("[ req ] {}<xdg_toplevel>::set_fullscreen(output={})", obj,arg_output);
    is_fullscreen = true;
    return bytes_t{};
}

export bytes_t xdg_unset_fullscreen(wire::object_t obj, bytes_t args)
{
    thread_logger().debug("[ req ] {}<xdg_toplevel>::unset_fullscreen()", obj);
    is_fullscreen = false;
    return bytes_t{};
}

export bytes_t logical_size(wire::object_t obj, bytes_t args)
{
    assert(args.size() < 1024);
    std::memcpy(buf.data(), args.data(), args.size());
    auto args_orig = args;

    wire::int_t arg_width;
    wire::int_t arg_height;
    auto width_pos = args.data() - args_orig.data();
    std::tie(arg_width, args) = ib::read_int(args);
    auto heigth_pos = args.data() - args_orig.data();
    std::tie(arg_height, args) = ib::read_int(args);
    thread_logger().debug("[! ev !] {}<zxdg_output_v1>::logical_size(width={},height={})", obj,arg_width,arg_height);

    write_int(buf.data() + width_pos, half_width);
    write_int(buf.data() + heigth_pos, half_height);
    thread_logger().debug("[event] {}<zxdg_output_v1>::logical_size(width={},height={})", obj,half_width,half_height);
    return {buf.data(), args_orig.size()};
}

export bytes_t set_cursor_position_hint(wire::object_t obj, bytes_t args)
{
    assert(args.size() < 1024);
    std::memcpy(buf.data(), args.data(), args.size());
    auto args_orig = args;

    wire::fixed_t arg_surface_x;
    wire::fixed_t arg_surface_y;
    auto surface_x_pos = args.data() - args_orig.data();
    std::tie(arg_surface_x, args) = ib::read_fixed(args);
    auto surface_y_pos = args.data() - args_orig.data();
    std::tie(arg_surface_y, args) = ib::read_fixed(args);
    thread_logger().debug("[!req!] {}<zwp_locked_pointer_v1>::set_cursor_position_hint(surface_x={},surface_y={})", obj,arg_surface_x,arg_surface_y);

    double ratio = (double)host_scaled_width / (double)half_width;
    arg_surface_x = wire::fixed_t{arg_surface_x.double_repr * ratio};
    arg_surface_y = wire::fixed_t{arg_surface_y.double_repr * ratio};
    write_fixed(buf.data() + surface_x_pos, arg_surface_x);
    write_fixed(buf.data() + surface_y_pos, arg_surface_y);
    thread_logger().debug("[!req!] {}<zwp_locked_pointer_v1>::set_cursor_position_hint(surface_x={},surface_y={})", obj,arg_surface_x,arg_surface_y);
    return {buf.data(), args_orig.size()};
}
