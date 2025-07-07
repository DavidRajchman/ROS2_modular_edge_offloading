# GitHub Copilot Custom Instructions for the Modular Gateway Project - POINT 3 IS EXTREMELY IMPORTANT

## 1. Core Project Context

-   **Project:** This is a C++ ROS 2 project named `modular_gateway_sender`.
-   **Platform:** The development environment is Ubuntu 24.04. All file paths and system commands should be compatible with this environment.
-   **Primary Goal:** Your primary goal is to assist in developing the Modular Gateway. All code suggestions must be consistent with the existing architecture, which includes a `data_plane` (`RosGateway`) and a `control_plane` (`GatewayController`, `DiscoveryClient`, etc.).

## 2. C++ Coding Style and Conventions

-   **Logging:** Do not use `std::cout`, `printf`, or the standard `rclcpp` logger. This project uses a custom `CppLogging` library.
    -   Include it with this statement #include "logging/logger.h"
    -   The library must be configured once in and that is already done in the project
    -   Obtain a logger instance via `logger_ = CppLogging::Logger("gateway");`.
    -   Use the `fmt`-style methods for logging (e.g., `logger_.Info("Message: {}", value)`).
    -   Every log message must include destinctive information about which file the log message is from here is an example: `logger_.Info("ros_gateway.cpp: Gateway destructor called");`
-   **Memory Management:** Strictly use smart pointers (`std::unique_ptr` and `std::shared_ptr`) for managing the lifecycle of objects and resources. Avoid owning raw pointers.
-   **ROS 2 Integration:** Adhere to `rclcpp` best practices.
    -   Nodes should inherit from `rclcpp::Node`.
    -   Configuration should be managed via ROS 2 parameters (`declare_parameter`, `get_parameter`).
    -   Inter-node communication should use ROS 2 topics, services, and actions.
-   **Headers:**
    -   Use `#ifndef FILENAME_HPP` style include guards.
    -   To reduce compile times and dependencies, prefer forward-declaring classes in header files (`class MyClass;`) instead of including the full header whenever possible.
-   **Threading:** For concurrency, use the C++ standard library features (`std::thread`, `std::mutex`, `std::atomic`, `std::condition_variable`), following the patterns established in `GatewayController`.

## 3. Code Block Formatting and Presentation EXTREMELY IMPORTANT

This is a critical instruction to ensure that code suggestions are easy to use.

-   **Principle of Cohesion:** Your primary objective when suggesting code is to provide large, contiguous blocks that the user can copy and paste with minimal effort.
-   **The `...existing code...` Rule:**
    -   You are **only** permitted to use the `// ...existing code...` marker at the very beginning or the very end of a Markdown code block.
    -   You are **prohibited** from using `// ...existing code...` in the middle of a code block to skip lines.
-   **The Merging Rule:**
    -   If you are suggesting two or more changes to a file that are separated by **fewer than 20 lines** of original, unchanged code, you **must** present these changes in a **single code block**.
    -   You must include the original, unchanged lines that fall between your modifications within that single block.
-   **The Splitting Rule:**
    -   You may only split your suggestions into multiple code blocks if the distance between the modifications is **20 lines or more**, or if the split occurs at a major logical boundary (e.g., the end of a complete function or class definition).

### Example of Correct Formatting (Merge Rule)

**Correct:**
````cpp
// filepath: /path/to/file.cpp
// ...existing code...
void MyClass::some_function()
{
    int x = 1; // Your new code

    // This is existing code that was less than 20 lines away
    // and must be included in the same block.
    if (x > 0) {
        call_another_function();
    }

    int y = 2; // Your second piece of new code
}
// ...existing code...
````

**Incorrect:**
````cpp
// filepath: /path/to/file.cpp
// ...existing code...
void MyClass::some_function()
{
    int x = 1; // Your new code
// ...existing code...
    int y = 2; // Your second piece of new code
}
// ...existing code...
````

By following these instructions, you will provide code that is stylistically consistent with the project and easy for me to integrate.
