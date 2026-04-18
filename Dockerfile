FROM ghcr.io/wiiu-env/devkitppc:20260225

COPY --from=ghcr.io/wiiu-env/libmocha:20260331 /artifacts $DEVKITPRO
COPY --from=ghcr.io/stroopwafelcfw/libstroopwafel:20260202 /artifacts $DEVKITPRO

WORKDIR /project
