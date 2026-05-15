# SC4 Season Jumper

![SC4SeasonJumper logo](docs/assets/logo.png)

`SC4SeasonJumper` is a SimCity 4 DLL plugin that advances the simulation at the
game's fastest speed until a target season begins, then pauses the city. Because
the simulator is allowed to run normally it does not cause issues with historical
city stats, unlike the SC4ExtraCheatsDLL `SimDate` cheat code.

The plugin is mainly meant to make two common date-sensitive workflows easier:
using seasonal flora, which should be planted on September 1st for consistent
results, and saving cities on June 1st so region view thumbnails use a consistent
summer date.

## Installation

Copy these files into your SimCity 4 `Plugins` folder:

- `SC4SeasonJumper.dll`
- `SC4SeasonJumper.ini`
- `SC4SeasonJumper.dat`

## Controls

The hotkey is configured by the companion WinKey DAT resource:

- `Ctrl+F9`: jump to the next fall start if the current date is January through
  August, or to the next summer start if the current date is September through
  December.
- Press `Ctrl+F9` again while a jump is running to cancel and pause immediately.

The plugin also registers these cheat codes through the normal `Ctrl+X` cheat
dialog:

- `jumpseason`: same behavior as the hotkey.
- `jumpspring`: next March 1.
- `jumpsummer`: next June 1.
- `jumpfall`: next September 1.
- `jumpwinter`: next December 1.

The keyboard shortcut can be changed by editing `SC4SeasonJumper.dat` with
iLive's Reader.

## Configuration

The default INI file is copied to the Plugins folder on first deployment:

```ini
[SC4SeasonJumper]
LogLevel=info
LogToFile=true
```

`LogLevel` accepts `trace`, `debug`, `info`, `warn`, `error`, `critical`, or
`off`.

## Building

This project targets the 32-bit Windows version of SimCity 4.

```powershell
cmake --preset vs2022-win32-debug
cmake --build --preset vs2022-win32-debug-build
```

The build deploys `SC4SeasonJumper.dll` and the default INI to:

```text
%USERPROFILE%\Documents\SimCity 4\Plugins
```

Disable deployment with:

```powershell
cmake --preset vs2022-win32-debug -DSC4_ENABLE_PLUGIN_DEPLOYMENT=OFF
```
