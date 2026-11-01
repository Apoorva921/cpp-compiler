FROM node:18-bullseye-slim

# Install GCC, G++, 32-bit multilib libraries, and Make from Debian official repos
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc \
    g++ \
    gcc-multilib \
    g++-multilib \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy compiler source code
COPY Makefile ./
COPY src ./src
COPY examples ./examples

# Build native compiler for Linux
RUN make && chmod +x mcxx

# Copy web application
COPY web ./web

WORKDIR /app/web

ENV PORT=3000
EXPOSE 3000

CMD ["node", "server.js"]
