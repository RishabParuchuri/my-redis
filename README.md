# 🔧 MiniRedis
A high-performance, Redis-inspired in-memory key-value store engineered entirely in modern C++. Designed for clarity and speed, this project features a custom binary protocol, a low-level TCP server leveraging non-blocking I/O with `poll()`, and an efficient standalone client — all built from the ground up with zero external dependencies.

---

## 🚀 Features

- 🧵 Event-driven architecture using `poll()` for scalable I/O  
  Handles multiple client connections concurrently in a single thread using low-level polling.

- 🔌 Built from scratch with raw socket programming  
  No frameworks or libraries — just `socket()`, `bind()`, `accept()`, `read()`, and `write()`.

- 💬 Custom length-prefixed binary protocol for minimal overhead  
  Compact and efficient message framing designed for fast parsing.

- 🧠 Core Redis-like commands: `GET`, `SET`, `DEL`  
  Supports essential key-value operations with constant-time access.

- 🛠 Fully self-contained C++ client for testing and interaction  
  Easily test commands from the terminal using the included CLI client.


## 📁 Project Structure

```bash
my-redis/
├── client.cpp      # Command-line client
├── server.cpp      # TCP server
├── Makefile        # Build automation
├── README.md       # You're reading this
```
## 🛠 Build Instructions

Make sure you have a C++ compiler (like `g++`) installed.

```bash
# Build the project
make

# Clean build artifacts
make clean
```
## 🧪 Usage

### Start the Server

```bash
./server
```
The server listens on 127.0.0.1:1234.
### Use the Client
```bash
# Set a key
./client set foo bar

# Get the key
./client get foo

# Delete the key
./client del foo

# Try getting a non-existent key
./client get nope
```
## 📦 Supported Commands

| Command        | Description                      |
|----------------|----------------------------------|
| `SET key val`  | Stores a value for a given key   |
| `GET key`      | Retrieves the value of a key     |
| `DEL key`      | Deletes the given key            |


