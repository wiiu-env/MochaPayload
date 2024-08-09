FROM ghcr.io/wiiu-env/devkitppc:20240704

COPY --from=ghcr.io/wiiu-env/libmocha:20231127 /artifacts $DEVKITPRO

WORKDIR project