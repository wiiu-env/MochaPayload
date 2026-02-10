FROM ghcr.io/wiiu-env/devkitppc:20260204

COPY --from=ghcr.io/wiiu-env/libmocha:20260126 /artifacts $DEVKITPRO
COPY --from=ghcr.io/stroopwafelcfw/libstroopwafel:20260131 /artifacts $DEVKITPRO

WORKDIR /project
