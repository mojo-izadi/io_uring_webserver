FROM ubuntu:latest

# Install required dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    liburing-dev

# Copy server code into the container
COPY server.c /server.c

# Compile the server code
RUN gcc -o server /server.c -luring

# Set the entrypoint command to run the server
CMD ["./server"]