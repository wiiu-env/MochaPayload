FROM wiiuenv/devkitppc:20220806

COPY --from=wiiuenv/libmocha:20220728 /artifacts $DEVKITPRO

WORKDIR project