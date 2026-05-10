cmake_minimum_required(VERSION 4.2)

CPMAddPackage("gh:gabime/spdlog@1.17.0")
CPMAddPackage("gh:microsoft/GSL@4.2.1")

add_executable(wlmc
        wlmc.cpp
)

target_sources(wlmc PUBLIC
        FILE_SET CXX_MODULES FILES
        observer.cppm
        interface_base.cppm
        core_wayland.cppm
)

target_link_libraries(wlmc PRIVATE
        spdlog
        Microsoft.GSL::GSL
)

#CPMAddPackage("gh:zeux/pugixml@1.15")
#
#add_executable(proto-gen generator.cpp)
#target_link_libraries(proto-gen PRIVATE
#        pugixml::pugixml
#        spdlog
#        Microsoft.GSL::GSL
#)
