# Use the official GCC image
FROM gcc:13

# Create working directory
WORKDIR /app

# Copy everything into the container
COPY . .

# Build server and client using Makefile
RUN make

# Expose the port your server listens on
EXPOSE 1234

# Default command — used for the server
CMD ["./server"]
