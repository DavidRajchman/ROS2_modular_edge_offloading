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
    # Network debugging tools
    iproute2 \
    iputils-ping \
    net-tools \
    netcat \
    tcpdump \
    nmap \
    curl \
    wget \
    iptables \
    dnsutils \
    traceroute \
    # Additional useful tools
    htop \
    tree \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Create a user to match typical Ubuntu setup (optional, matches with your docker-compose)
RUN mkdir -p /etc/sudoers.d
RUN useradd -m ubuntu
RUN echo "ubuntu ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/ubuntu
RUN chmod 0440 /etc/sudoers.d/ubuntu

USER ubuntu
WORKDIR /home/ubuntu/bridge

CMD ["/bin/bash"]

