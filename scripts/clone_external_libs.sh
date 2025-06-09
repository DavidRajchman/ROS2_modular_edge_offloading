#!/bin/bash
set -e # Exit immediately if a command exits with a non-zero status.

# --- Configuration ---
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
EXTERNAL_LIBS_DIR_HOST_ABS="${SCRIPT_DIR}/../external_libs" # Absolute path to external_libs

# CppCommon
CPPCOMMON_LIB_NAME="CppCommon"
CPPCOMMON_GIT_URL="https://github.com/chronoxor/CppCommon.git"
CPPCOMMON_PATH_HOST_ABS="${EXTERNAL_LIBS_DIR_HOST_ABS}/${CPPCOMMON_LIB_NAME}"

# CppLogging
CPPLOGGING_LIB_NAME="CppLogging"
CPPLOGGING_GIT_URL="https://github.com/chronoxor/CppLogging.git"
CPPLOGGING_PATH_HOST_ABS="${EXTERNAL_LIBS_DIR_HOST_ABS}/${CPPLOGGING_LIB_NAME}"

# moodycamel::ConcurrentQueue
MOODYCAMEL_LIB_NAME="moodycamel_concurrent_queue"
MOODYCAMEL_GIT_URL="https://github.com/cameron314/concurrentqueue.git"
MOODYCAMEL_GIT_TAG="v1.0.4"
MOODYCAMEL_PATH_HOST_ABS="${EXTERNAL_LIBS_DIR_HOST_ABS}/${MOODYCAMEL_LIB_NAME}"

# fmt library
FMT_LIB_NAME="fmt"
FMT_GIT_URL="https://github.com/fmtlib/fmt.git"
FMT_GIT_TAG="11.2.0" # Specific tag for fmt
FMT_PATH_HOST_ABS="${EXTERNAL_LIBS_DIR_HOST_ABS}/${FMT_LIB_NAME}"
# --- End Configuration ---

echo "--- Cloning External Libraries ---"
mkdir -p "${EXTERNAL_LIBS_DIR_HOST_ABS}"

# Function to clone or update a repository
clone_or_update_repo() {
    local name="$1"
    local url="$2"
    local path="$3"
    local tag="${4:-}" # Optional tag

    echo ""
    echo "--- Setting up ${name} ---"
    if [ ! -d "${path}/.git" ]; then
        echo "Cloning ${name} from ${url} into ${path}..."
        git clone "${url}" "${path}"
        if [ -n "$tag" ]; then
            (cd "${path}" && git checkout "${tag}")
            echo "Checked out tag ${tag} for ${name}."
        fi
    else
        echo "${name} directory already exists. Fetching updates..."
        (cd "${path}" && git fetch --all)
        if [ -n "$tag" ]; then
            (cd "${path}" && git checkout "${tag}")
            echo "Checked out tag ${tag} for ${name}."
        else
            # If no specific tag, you might want to pull the default branch
            # (cd "${path}" && git pull) # Or leave as is to be handled by Docker build if needed
            echo "${name} is present. Specific version/tag will be handled if specified."
        fi
    fi
}

clone_or_update_repo "${CPPCOMMON_LIB_NAME}" "${CPPCOMMON_GIT_URL}" "${CPPCOMMON_PATH_HOST_ABS}"
clone_or_update_repo "${CPPLOGGING_LIB_NAME}" "${CPPLOGGING_GIT_URL}" "${CPPLOGGING_PATH_HOST_ABS}"
clone_or_update_repo "${MOODYCAMEL_LIB_NAME}" "${MOODYCAMEL_GIT_URL}" "${MOODYCAMEL_PATH_HOST_ABS}" "${MOODYCAMEL_GIT_TAG}"
clone_or_update_repo "${FMT_LIB_NAME}" "${FMT_GIT_URL}" "${FMT_PATH_HOST_ABS}" "${FMT_GIT_TAG}"

echo ""
echo "--- External Libraries Cloning Complete ---"
echo "Sources are in: ${EXTERNAL_LIBS_DIR_HOST_ABS}"
echo "Next, build your Docker images. The Dockerfiles will compile these libraries."