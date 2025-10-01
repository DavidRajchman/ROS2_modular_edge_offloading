#!/bin/bash
set -e # Exit immediately if a command exits with a non-zero status.

# --- Configuration ---
EXTERNAL_LIBS_DIR_HOST_ABS="$(cd "$(dirname "$0")/.." && pwd)/external_libs" # Absolute path to external_libs

# CppCommon
CPPCOMMON_LIB_NAME="CppCommon"
CPPCOMMON_GIT_URL="https://github.com/chronoxor/CppCommon.git"
CPPCOMMON_PATH_HOST_ABS="${EXTERNAL_LIBS_DIR_HOST_ABS}/${CPPCOMMON_LIB_NAME}"
CPPCOMMON_BUILT_FLAG_FILE="${CPPCOMMON_PATH_HOST_ABS}/.built_successfully" # Flag to check if built

# CppLogging
CPPLOGGING_LIB_NAME="CppLogging"
CPPLOGGING_GIT_URL="https://github.com/chronoxor/CppLogging.git"
CPPLOGGING_PATH_HOST_ABS="${EXTERNAL_LIBS_DIR_HOST_ABS}/${CPPLOGGING_LIB_NAME}"
CPPLOGGING_BUILT_FLAG_FILE="${CPPLOGGING_PATH_HOST_ABS}/.built_successfully" # Flag to check if built

# moodycamel::ConcurrentQueue
MOODYCAMEL_LIB_NAME="moodycamel_concurrent_queue"
MOODYCAMEL_GIT_URL="https://github.com/cameron314/concurrentqueue.git"
MOODYCAMEL_GIT_TAG="v1.0.4" # As used previously, adjust if needed
MOODYCAMEL_PATH_HOST_ABS="${EXTERNAL_LIBS_DIR_HOST_ABS}/${MOODYCAMEL_LIB_NAME}"

# fmt library
FMT_LIB_NAME="fmt"
FMT_GIT_URL="https://github.com/fmtlib/fmt.git"
FMT_GIT_TAG="11.2.0" # Specific tag for fmt
FMT_PATH_HOST_ABS="${EXTERNAL_LIBS_DIR_HOST_ABS}/${FMT_LIB_NAME}"
FMT_INSTALL_PREFIX_HOST_ABS="${FMT_PATH_HOST_ABS}/install" # Local install directory
FMT_BUILT_FLAG_FILE="${FMT_INSTALL_PREFIX_HOST_ABS}/.built_successfully" # Flag to check if built
# --- End Configuration ---

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check if a deb package is installed
package_installed() {
    dpkg-query -W -f='${Status}' "$1" 2>/dev/null | grep -q "ok installed"
}

echo "--- Preparing External Libraries ---"
mkdir -p "${EXTERNAL_LIBS_DIR_HOST_ABS}"

# 1. Check System Dependencies (for CppCommon and CppLogging)
echo ""
echo "--- Checking system dependencies (for CppCommon & CppLogging) ---"
SYSTEM_DEPS_MET=true
if ! package_installed binutils-dev; then
    echo "ERROR: 'binutils-dev' is not installed. Please install it using: sudo apt-get install -y binutils-dev"
    SYSTEM_DEPS_MET=false
fi
if ! package_installed uuid-dev; then
    echo "ERROR: 'uuid-dev' is not installed. Please install it using: sudo apt-get install -y uuid-dev"
    SYSTEM_DEPS_MET=false
fi
# fmt generally doesn't have extra system deps beyond a C++ compiler and CMake
if ! command_exists cmake; then
    echo "ERROR: 'cmake' is not installed. Please install it using: sudo apt-get install -y cmake"
    SYSTEM_DEPS_MET=false
fi


if [ "$SYSTEM_DEPS_MET" = false ]; then
    echo "Please install the missing system dependencies and re-run this script."
    exit 1
fi
echo "System dependencies are satisfied."

# 2. Check and Install pip3 and gil
echo ""
echo "--- Checking for pip3 and gil (for CppCommon & CppLogging) ---"
if ! command_exists pip3; then
    echo "pip3 not found. Attempting to install python3-pip..."
    sudo apt-get update
    sudo apt-get install -y python3-pip
    if ! command_exists pip3; then
        echo "ERROR: Failed to install pip3. Please install it manually."
        exit 1
    fi
fi

if ! pip3 show gil >/dev/null 2>&1; then
    echo "gil (git links) not found. Installing using pip3..."
    pip3 install --user gil # Install for the current user
    # Add ~/.local/bin to PATH if it's not already there for the current session and future ones
    if [[ ":$PATH:" != *":$HOME/.local/bin:"* ]]; then
        echo "Adding $HOME/.local/bin to PATH for this session."
        export PATH="$HOME/.local/bin:$PATH"
        echo "Please ensure $HOME/.local/bin is in your PATH for future sessions."
        echo "You might need to add 'export PATH=\"\$HOME/.local/bin:\$PATH\"' to your ~/.bashrc or ~/.zshrc"
    fi
    if ! command_exists gil; then # Check if gil is now in PATH
         # Try to find it in .local/bin explicitly if PATH not updated yet for this script instance
        if [ -x "$HOME/.local/bin/gil" ]; then
            GIL_CMD="$HOME/.local/bin/gil"
        else
            echo "ERROR: gil installed but not found in PATH. Please check your PATH or run 'source ~/.bashrc'."
            exit 1
        fi
    else
        GIL_CMD="gil"
    fi
else
    echo "gil is already installed."
    GIL_CMD="gil"
fi

# 3. Setup CppCommon
echo ""
echo "--- Setting up ${CPPCOMMON_LIB_NAME} ---"
if [ ! -d "${CPPCOMMON_PATH_HOST_ABS}" ]; then
    echo "Cloning ${CPPCOMMON_LIB_NAME} from ${CPPCOMMON_GIT_URL}..."
    git clone "${CPPCOMMON_GIT_URL}" "${CPPCOMMON_PATH_HOST_ABS}"
else
    echo "${CPPCOMMON_LIB_NAME} directory already exists. Updating..."
    (cd "${CPPCOMMON_PATH_HOST_ABS}" && git pull)
fi

echo "Updating CppCommon dependencies with gil..."
(cd "${CPPCOMMON_PATH_HOST_ABS}" && ${GIL_CMD} update)

if [ ! -f "${CPPCOMMON_BUILT_FLAG_FILE}" ]; then
    echo "Building ${CPPCOMMON_LIB_NAME}..."
    (cd "${CPPCOMMON_PATH_HOST_ABS}/build" && ./unix.sh) # Assuming unix.sh handles Release build
    touch "${CPPCOMMON_BUILT_FLAG_FILE}"
    echo "${CPPCOMMON_LIB_NAME} built successfully."
else
    echo "${CPPCOMMON_LIB_NAME} appears to be already built. Skipping build."
fi

# 4. Setup CppLogging
echo ""
echo "--- Setting up ${CPPLOGGING_LIB_NAME} ---"
if [ ! -d "${CPPLOGGING_PATH_HOST_ABS}" ]; then
    echo "Cloning ${CPPLOGGING_LIB_NAME} from ${CPPLOGGING_GIT_URL}..."
    git clone "${CPPLOGGING_GIT_URL}" "${CPPLOGGING_PATH_HOST_ABS}"
else
    echo "${CPPLOGGING_LIB_NAME} directory already exists. Updating..."
    (cd "${CPPLOGGING_PATH_HOST_ABS}" && git pull)
fi

echo "Updating CppLogging dependencies with gil..."
(cd "${CPPLOGGING_PATH_HOST_ABS}" && ${GIL_CMD} update)

if [ ! -f "${CPPLOGGING_BUILT_FLAG_FILE}" ]; then
    echo "Building ${CPPLOGGING_LIB_NAME}..."
    (cd "${CPPLOGGING_PATH_HOST_ABS}/build" && ./unix.sh) # Assuming unix.sh handles Release build
    touch "${CPPLOGGING_BUILT_FLAG_FILE}"
    echo "${CPPLOGGING_LIB_NAME} built successfully."
else
    echo "${CPPLOGGING_LIB_NAME} appears to be already built. Skipping build."
fi

# 5. Setup moodycamel::ConcurrentQueue
echo ""
echo "--- Setting up ${MOODYCAMEL_LIB_NAME} ---"
if [ ! -d "${MOODYCAMEL_PATH_HOST_ABS}" ]; then
    echo "Cloning ${MOODYCAMEL_LIB_NAME} from ${MOODYCAMEL_GIT_URL}..."
    git clone "${MOODYCAMEL_GIT_URL}" "${MOODYCAMEL_PATH_HOST_ABS}"
    (cd "${MOODYCAMEL_PATH_HOST_ABS}" && git checkout "${MOODYCAMEL_GIT_TAG}")
else
    echo "${MOODYCAMEL_LIB_NAME} directory already exists. Fetching and checking out tag ${MOODYCAMEL_GIT_TAG}..."
    (cd "${MOODYCAMEL_PATH_HOST_ABS}" && git fetch --all && git checkout "${MOODYCAMEL_GIT_TAG}")
fi
echo "${MOODYCAMEL_LIB_NAME} (header-only) is ready."

# 6. Setup fmt library
echo ""
echo "--- Setting up ${FMT_LIB_NAME} ---"
# Create the specific directory for fmt if it doesn't exist
mkdir -p "${FMT_PATH_HOST_ABS}"

if [ ! -d "${FMT_PATH_HOST_ABS}/.git" ]; then # Check if it's a git repo, clone if not
    echo "Cloning ${FMT_LIB_NAME} from ${FMT_GIT_URL} into ${FMT_PATH_HOST_ABS}..."
    # Clone into the directory itself, requires it to be empty or not exist for 'git clone . '
    # Safer to clone to a temp name then move, or ensure it's empty.
    # For simplicity, we'll assume if .git isn't there, we can clone into it.
    # If the directory was created but empty, 'git clone . ' works.
    git clone "${FMT_GIT_URL}" "${FMT_PATH_HOST_ABS}" # This clones into a subdir named fmt, let's adjust
fi
# The user's command was `git clone https://github.com/fmtlib/fmt.git .` AFTER `cd external_libs/fmt`
# So we ensure the directory exists, then clone into it.

# If the directory exists but is not a git repo (e.g. only `mkdir -p` ran)
# or if we want to ensure we are in the correct state.
if [ ! -d "${FMT_PATH_HOST_ABS}/.git" ]; then
    echo "Re-cloning ${FMT_LIB_NAME} as it was not properly initialized..."
    rm -rf "${FMT_PATH_HOST_ABS:?}"/* "${FMT_PATH_HOST_ABS:?}"/.[!.]* # Clean directory contents
    git clone "${FMT_GIT_URL}" "${FMT_PATH_HOST_ABS}" # Clone directly into the target dir
fi

echo "Fetching and checking out tag ${FMT_GIT_TAG} for ${FMT_LIB_NAME}..."
(cd "${FMT_PATH_HOST_ABS}" && git fetch --all && git checkout "${FMT_GIT_TAG}")


if [ ! -f "${FMT_BUILT_FLAG_FILE}" ]; then
    echo "Building ${FMT_LIB_NAME}..."
    (cd "${FMT_PATH_HOST_ABS}" && \
     rm -rf build && \
     mkdir build && \
     cd build && \
     cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="${FMT_INSTALL_PREFIX_HOST_ABS}" && \
     make -j$(nproc) && \
     make install) # No sudo needed due to CMAKE_INSTALL_PREFIX

    mkdir -p "${FMT_INSTALL_PREFIX_HOST_ABS}" # Ensure install dir exists for the flag
    touch "${FMT_BUILT_FLAG_FILE}"
    echo "${FMT_LIB_NAME} built and installed successfully to ${FMT_INSTALL_PREFIX_HOST_ABS}."
else
    echo "${FMT_LIB_NAME} appears to be already built. Skipping build."
fi


echo ""
echo "--- External Libraries Preparation Complete ---"
echo "CppCommon is in: ${CPPCOMMON_PATH_HOST_ABS}"
echo "CppLogging is in: ${CPPLOGGING_PATH_HOST_ABS}"
echo "moodycamel::ConcurrentQueue is in: ${MOODYCAMEL_PATH_HOST_ABS}"
echo "fmt is in: ${FMT_PATH_HOST_ABS} (installed to ${FMT_INSTALL_PREFIX_HOST_ABS})"
echo "Ensure these paths are correctly referenced in your CMakeLists.txt files for the containers."
