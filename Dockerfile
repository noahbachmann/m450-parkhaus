# PC simulator of both Picos (see README, "PC-Simulator"). Builds src/sim/ + lib/ with g++; no PlatformIO needed.
FROM gcc:13-bookworm AS build
ADD https://github.com/bblanchon/ArduinoJson/releases/download/v7.4.3/ArduinoJson-v7.4.3.h /deps/ArduinoJson.h
WORKDIR /build
COPY lib lib
COPY src/sim src/sim
RUN mkdir /out && INC="-I/deps -Isrc/sim/common $(for d in lib/*/; do printf -- '-I%s ' "$d"; done)" && \
    for p in kasse schranken; do \
        g++ -std=gnu++17 -O2 -static-libstdc++ -static-libgcc $INC \
            src/sim/common/*.cpp src/sim/$p/main.cpp lib/*/*.cpp -o /out/sim_$p || exit 1; \
    done

FROM debian:bookworm-slim
COPY --from=build /out/ /usr/local/bin/
ENV SIM_HOST=0.0.0.0 SIM_DATA=/data
VOLUME /data
CMD ["sim_kasse"]
