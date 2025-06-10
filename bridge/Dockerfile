FROM ubuntu:22.04

# Avoid prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install C++ development tools, network debugging utilities,
# and dependencies for external libraries
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    gdb \
    git \
    vim \
    nano \
    pkg-config \
    libboost-all-dev \
    sudo \
    iputils-ping \
    iproute2 \
    python3-pip \
    binutils-dev \
    uuid-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Install gil (used by CppCommon and CppLogging)
RUN pip3 install gil

# --- Setup External Libraries (CppCommon, CppLogging, fmt, moodycamel) ---
# Copy the cloned library sources from the host into /home/ubuntu/external_libs
# The build context for the 'bridge' service in docker-compose.yml is '.',
# so ./external_libs refers to the external_libs directory at the project root.
COPY ./external_libs /home/ubuntu/external_libs

# --- Build CppCommon ---
WORKDIR /home/ubuntu/external_libs/CppCommon
RUN gil update && \
    cd build && \
    ./unix.sh

# --- Build CppLogging ---
WORKDIR /home/ubuntu/external_libs/CppLogging
RUN gil update && \
    cd build && \
    ./unix.sh

# --- Build and Install fmt ---
## Install fmt system-wide to /usr/local
WORKDIR /home/ubuntu/external_libs/fmt
RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local -DFMT_TEST=OFF && \
    make -j$(nproc) && \
    make install
# fmt is now "installed" to /home/ubuntu/external_libs/fmt/install_local
# moodycamel::ConcurrentQueue is header-only and is at /home/ubuntu/external_libs/moodycamel_concurrent_queue
# --- End External Libraries Setup ---

# --- Build and Install TransportLib (Your existing library) ---
COPY ./cpplib /home/ubuntu/cpplib_src_build_time
WORKDIR /home/ubuntu/cpplib_src_build_time/transport
RUN rm -rf build && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make -j$(nproc) && \
    make install
# Optional: RUN rm -rf /home/ubuntu/cpplib_src_build_time
# --- End TransportLib Installation ---

RUN mkdir -p /etc/sudoers.d && \
    useradd -m ubuntu && \
    echo "ubuntu ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/ubuntu && \
    chmod 0440 /etc/sudoers.d/ubuntu && \
    chown -R ubuntu:ubuntu /home/ubuntu # Ensure ubuntu owns its home directory

# Switch to ubuntu user for subsequent commands and default runtime user
USER ubuntu

# Set back to the original working directory for the bridge application
WORKDIR /home/ubuntu/bridge

# Default command to start a bash shell
CMD ["/bin/bash"]