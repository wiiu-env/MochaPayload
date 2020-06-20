# Mocha lite - a simple custom firmware
This a lite version of the [original mocha](https://github.com/dimok789/mocha) to be used with the [SetupPayload](https://github.com/wiiu-env/SetupPayload).

## Usage
Place the `00_mocha.rpx` in the `sd:/wiiu/modules/setup` folder and run the [SetupPayload](https://github.com/wiiu-env/SetupPayload).
Requires [PayloadFromRPX](https://github.com/wiiu-env/PayloadFromRPX) as `sd:/wiiu/root.rpx` to support returning from the system settings.

## Patches
- You can also place a RPX as `men.rpx` in the `sd:/wiiu` folder which will replace the Wii U Menu.
- RPX redirection
- overall sd access
- wupserver and own IPC which can be used with [libiosuhax](https://github.com/wiiu-env/libiosuhax).
- Opening the "Health and Safety" application will try to run the [Homebrew Launcher](https://github.com/dimok789/homebrew_launcher/) from `sd:/wiiu/apps/homebrew_launcher/homebrew_launcher.rpx` from sd card.

## Building

For building you just need [wut](https://github.com/devkitPro/wut/) installed, then use the `make` command.

## Credits
dimok
Maschell
orboditilt
QuarkTheAwesome