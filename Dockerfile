FROM gcc:12-bullseye

# Install Node.js 18
RUN apt-get update && apt-get install -y curl gcc-multilib g++-multilib make \
    && curl -fsSL https://deb.nodesource.com/setup_18.x | bash - \
    && apt-get install -y nodejs \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy compiler source code
COPY Makefile ./
COPY src ./src
COPY examples ./examples
COPY test_sample.cpp ./

# Build mcxx compiler for Linux
RUN make

# Copy web app
COPY web ./web

WORKDIR /app/web

EXPOSE 3000

CMD ["node", "server.js"]
