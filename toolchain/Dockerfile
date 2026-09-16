FROM ubuntu:26.04
RUN apt-get update && apt-get install -y make cmake cmake-curses-gui ninja-build pkg-config
RUN apt-get install -y clang-22 clang-tools-22 clang-format-22 clangd-22 clang-tidy-22
RUN apt-get install -y git g++-16

RUN update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-22 100 && \
    update-alternatives --install /usr/bin/clang clang /usr/bin/clang-22 100 && \
    update-alternatives --install /usr/bin/clangd clangd /usr/bin/clangd-22 100 && \
    update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-22 100 && \
    update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-22 100
