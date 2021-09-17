FROM wiiuenv/devkitppc:20210917

COPY --from=wiiuenv/devkitarm:20210917 $DEVKITPRO/devkitARM $DEVKITPRO/devkitARM
# RUN dkp-pacman -Syu devkitARM --noconfirm

ENV DEVKITARM=/opt/devkitpro/devkitARM

WORKDIR project