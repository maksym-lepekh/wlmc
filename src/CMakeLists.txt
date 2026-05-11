cmake_minimum_required(VERSION 4.2)

CPMAddPackage("gh:gabime/spdlog@1.17.0")
CPMAddPackage("gh:microsoft/GSL@4.2.1")
CPMAddPackage("gh:zeux/pugixml@1.15")

add_executable(proto-gen generator.cpp)
target_link_libraries(proto-gen PRIVATE
        pugixml::pugixml
        spdlog
        Microsoft.GSL::GSL
)

function(add_generated_protocol arg_TARGET arg_INPUT_FILE)
    get_filename_component(proto_name "${arg_INPUT_FILE}" NAME_WLE)
    set(output "${CMAKE_CURRENT_BINARY_DIR}/proto/${proto_name}.cppm")

    add_custom_command(
            OUTPUT ${output}
            DEPENDS proto-gen "${arg_INPUT_FILE}"
            COMMAND proto-gen "${arg_INPUT_FILE}" "${output}"
            COMMENT "Generating ${output}..."
    )

    target_sources(${arg_TARGET} PUBLIC
            FILE_SET CXX_MODULES
            BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
            FILES ${output}
    )
endfunction()

add_executable(wlmc
        wlmc.cpp
)

target_sources(wlmc PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
        FILES
        observer.cppm
        interface_base.cppm
)

target_link_libraries(wlmc PRIVATE
        spdlog
        Microsoft.GSL::GSL
)

add_generated_protocol(wlmc ${CMAKE_SOURCE_DIR}/protocols/core/wayland.xml)
