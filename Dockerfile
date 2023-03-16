FROM ghcr.io/wiiu-env/devkitppc:20220806

COPY --from=ghcr.io/wiiu-env/libmocha:20220903 /artifacts $DEVKITPRO

WORKDIR project