FROM wiiuenv/devkitppc:20200625

COPY --from=devkitpro/devkitarm:20200528 $DEVKITPRO/devkitARM $DEVKITPRO/devkitARM
# RUN dkp-pacman -Syu devkitARM --noconfirm

ENV DEVKITARM=/opt/devkitpro/devkitARM

WORKDIR project