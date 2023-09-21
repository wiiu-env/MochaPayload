[![CI-Release](https://github.com/wiiu-env/MochaPayload/actions/workflows/ci.yml/badge.svg)](https://github.com/wiiu-env/MochaPayload/actions/workflows/ci.yml)

# MochaPayload - a simple custom firmware
This a version of the [original mocha](https://github.com/dimok789/mocha) to be used with the [EnvironmentLoader](https://github.com/wiiu-env/EnvironmentLoader).

## Usage
([ENVIRONMENT] is a placeholder for the actual environment name.)
1. Place the `00_mocha.rpx` in the `sd:/wiiu/environments/[ENVIRONMENT]/modules/setup` folder and run the [EnvironmentLoader](https://github.com/wiiu-env/EnvironmentLoader).
2. Requires [PayloadFromRPX](https://github.com/wiiu-env/PayloadFromRPX) as `sd:/wiiu/environments/[ENVIRONMENT]/root.rpx` to support returning from the system settings.

## Patches
- You can also place a RPX as `men.rpx` in the `sd:/wiiu/environments/[ENVIRONMENT]/` folder which will replace the Wii U Menu.
- RPX redirection
- overall sd access
- wupserver and own IPC which can be used with [libiosuhax](https://github.com/wiiu-env/libiosuhax).

## Building

For building you just need [wut](https://github.com/devkitPro/wut/), [libmocha](https://github.com/wiiu-env/libmocha), and devkitARM installed, then use the `make` command.

## Building using the Dockerfile

It's possible to use a docker image for building. This way you don't need anything installed on your host system.

```
# Build docker image (only needed once)
docker build . -t mochapayload-builder

# make 
docker run -it --rm -v ${PWD}:/project mochapayload-builder make

# make clean
docker run -it --rm -v ${PWD}:/project mochapayload-builder make clean
```

## Format the code via docker

`docker run --rm -v ${PWD}:/src ghcr.io/wiiu-env/clang-format:13.0.0-2 -r ./source -i`

## Credits
dimok
Maschell
orboditilt
QuarkTheAwesome