oci_tool := `which podman > /dev/null && echo podman || echo docker`
image_name := 'wlmc-toolchain:latest'


_default:
    @just --list

_run *ARGS:
    @{{oci_tool}} run --rm -it -u `id -u`:`id -g` -v `pwd`:/workspace -w /workspace {{image_name}} {{ARGS}}

# build toolchain image
build-toolchain:
    {{oci_tool}} build toolchain -t {{image_name}}

_build: ( _run 'cmake' '--workflow' '--preset=default' )

# build all tools
build: build-toolchain _build
