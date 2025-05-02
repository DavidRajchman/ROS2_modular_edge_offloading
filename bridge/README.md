# Bridge Application

A standalone C++ application for network bridging functionality.

## Building the Project

From the bridge directory:

```bash
mkdir -p build
cd build
cmake ..
make
```

## Running the Application

After building:

```bash
./bin/bridge
```

## Development

### Using Docker

The project includes Docker integration for a consistent development environment:

```bash
# Start the development container
docker-compose up bridge

# Build inside the container
cd /home/ubuntu/bridge
mkdir -p build && cd build
cmake ..
make
```

### Project Structure

- `include/`: Header files
- `src/`: Source files
- `build/`: Build output (generated)
- `bin/`: Executable output (generated)
