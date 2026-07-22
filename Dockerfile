FROM ubuntu:22.04 AS build
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake pkg-config default-libmysqlclient-dev libcurl4-openssl-dev \
    libavformat-dev libavcodec-dev libavutil-dev libswscale-dev ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSMART_HOME_BUILD_QT_CLIENT=OFF \
    && cmake --build build -j2 \
    && ctest --test-dir build --output-on-failure

FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    default-libmysqlclient-dev libcurl4 libavformat58 libavcodec58 libavutil56 \
    libstdc++6 ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /opt/smart-home
COPY --from=build /src/build/SmartHomeMonitorServer ./SmartHomeMonitorServer
COPY deploy/server.docker.conf ./conf/server.conf
RUN mkdir -p data log
EXPOSE 8000 8080
CMD ["./SmartHomeMonitorServer", "./conf/server.conf"]
