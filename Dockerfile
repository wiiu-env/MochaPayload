FROM wiiuenv/devkitppc:20220724

COPY --from=wiiuenv/libmocha:20220722 /artifacts $DEVKITPRO

WORKDIR project