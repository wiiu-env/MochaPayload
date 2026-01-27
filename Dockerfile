FROM ghcr.io/wiiu-env/devkitppc:20240505

COPY --from=ghcr.io/wiiu-env/libmocha:20231127 /artifacts $DEVKITPRO

RUN git clone https://github.com/jan-hofmeier/libstroopwafel && \
    cd libstroopwafel && \
    make install && \
    cd .. && \
    rm -rf libstroopwafel

WORKDIR project