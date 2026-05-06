# Build Optimization Proposals for P2P Mode

The current Docker build process is slow due to layer cache invalidation and redundant operations. The following improvements are proposed for future implementation.

## 1. Optimize Layer Ordering (High Impact)
Currently, `COPY ./src /home/ubuntu/` occurs on **line 12** in `ros/Dockerfile`, before the expensive `apt install` and library build steps.
- **Problem**: Any change to *any* source file invalidates all subsequent layers, forcing a full re-download of ~655 MB of packages and recompilation of C++ libraries.
- **Fix**: Move the global `COPY ./src` statement to after the library builds. Only copy specific files needed for early setup (like config files) as late as possible.

## 2. Pre-build ROS Workspace in Image
Currently, `colcon build` runs at container startup via `p2p_autostart.sh`.
- **Problem**: Compilation happens every time the container is started/restarted.
- **Fix**: Perform `colcon build` during the Docker image build process (uncomment/fix line 115). This moves the compilation cost to the build phase and allows it to be cached.

## 3. Multi-stage Builds or "Deps" Image
- **Fix**: Create a dedicated base image containing all system dependencies (apt packages) and pre-built C++ libraries (`CppCommon`, `CppLogging`, `fmt`, etc.).
- **Benefit**: The main application Dockerfile will start `FROM` this pre-prepared image, reducing individual build times to only the application logic.

## 4. Leverage BuildKit Cache Mounts
- **Fix**: Use `--mount=type=cache` for apt and pip directories.
- **Benefit**: Even if a layer is invalidated, the package manager will find already downloaded files in the local cache instead of re-fetching from the internet.

## 5. Unified Build Cache for Compose Services
- **Problem**: Both `ros2_vhc_p2p_ds` and `ros2_mec_p2p_ds` build from the same Dockerfile.
- **Fix**: Ensure `build.cache_from` is configured in `docker-compose.p2p_ds.yml` to guarantee they share built layers efficiently.
