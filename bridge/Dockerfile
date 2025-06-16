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
# Create a directory to hold the cloned sources
ARG EXTERNAL_LIBS_DIR=/home/ubuntu/external_libs
RUN mkdir -p ${EXTERNAL_LIBS_DIR}
WORKDIR ${EXTERNAL_LIBS_DIR}

# --- Clone CppCommon ---
RUN git clone https://github.com/chronoxor/CppCommon.git CppCommon && \
    cd CppCommon && \
    git checkout 1.0.5.0 && \
    gil update && \
    cd build && \
    ./unix.sh

# --- Clone CppLogging ---
# WORKDIR is still ${EXTERNAL_LIBS_DIR}
RUN git clone https://github.com/chronoxor/CppLogging.git CppLogging && \
    cd CppLogging && \
    git checkout 1.0.5.0 && \
    gil update && \
    cd build && \
    ./unix.sh

# --- Clone and Build fmt ---
# WORKDIR is still ${EXTERNAL_LIBS_DIR}
RUN git clone https://github.com/fmtlib/fmt.git fmt && \
    cd fmt && \
    git checkout 11.2.0 && \
    mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local -DFMT_TEST=OFF && \
    make -j$(nproc) && \
    make install
# fmt is now installed to /usr/local

# --- Clone moodycamel::ConcurrentQueue (header-only) ---
# WORKDIR is still ${EXTERNAL_LIBS_DIR}
RUN git clone https://github.com/cameron314/concurrentqueue.git moodycamel_concurrent_queue && \
    cd moodycamel_concurrent_queue && \
    git checkout v1.0.4
# moodycamel::ConcurrentQueue is header-only and is now at ${EXTERNAL_LIBS_DIR}/moodycamel_concurrent_queue
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