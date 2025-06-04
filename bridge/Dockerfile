FROM ubuntu:22.04

# Avoid prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install C++ development tools and network debugging utilities
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

    # sudo is already installed on ubuntu:22.04 base, but explicitly listing if needed by other tools
    # For the ubuntu user created below, sudo privileges are granted via /etc/sudoers.d/
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Create a user to match typical Ubuntu setup
RUN mkdir -p /etc/sudoers.d && \
    useradd -m ubuntu && \
    echo "ubuntu ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/ubuntu && \
    chmod 0440 /etc/sudoers.d/ubuntu

# --- Build and Install TransportLib ---
# Assuming the build context in docker-compose.yml is '.', so ./cpplib is correct.
COPY ./cpplib /home/ubuntu/cpplib_src_build_time 

# Set working directory for building TransportLib
WORKDIR /home/ubuntu/cpplib_src_build_time/transport

# Build and install TransportLib
# This build uses the source code copied into the image at build time.
# This RUN command executes as root, so sudo is not needed for make install.
# Ensure the build directory is clean before starting.
RUN rm -rf build && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make -j$(nproc) && \
    make install 

# Optional: Clean up the build-time source directory to save image space.
# If you are relying on the volume mount for the source code at runtime,
# removing this build-time copy can save a little space.
# However, if the volume mount is not present, the library would still be installed,
# but its source (from build time) would be gone.
# For maximum flexibility with volume mounts, it's often fine to leave it.
# RUN rm -rf /home/ubuntu/cpplib_src_build_time
# --- End TransportLib Installation ---

# Switch to ubuntu user for subsequent commands and default runtime user
USER ubuntu

# Set back to the original working directory for the bridge application
WORKDIR /home/ubuntu/bridge

# Default command to start a bash shell
CMD ["/bin/bash"]