﻿# 64Gram – Base on [Telegram Desktop](https://github.com/telegramdesktop/tdesktop)

The source code is published under GPLv3 with OpenSSL exception, the license is available [here][license].

[![Preview of 64Gram][preview_image]][preview_image_url]

## Project Goal

Provide Windows 64bit build with some enhancements.

~~Cause official Telegram Desktop do not provide Windows 64bit build, so [Project TDesktop x64](https://github.com/TDesktop-x64) is aimed at provide Windows native x64 build(with few enhancements) to everybody.~~

## Roadmap

No Roadmap? Yes.

## [Features](features.md)

## Supported systems

Windows 7 and above

Linux 64 bit

macOS > 10.12 and above

The latest version is available on the [Release](https://github.com/TDesktop-x64/tdesktop/releases) page.

## Localization

If you want to translate this project, **Just Do It!**

Create a Pull Request: [Localization Repo](https://github.com/TDesktop-x64/Localization).

**Here is a project [translation template](https://github.com/TDesktop-x64/Localization/blob/master/en.json).**

You can find a language ID on Telegram's log.txt

For example: `[2022.04.23 10:37:45] Current Language pack ID: de, Base ID: `

Then your language translation filename is `de.json` or something like that.

***Note: Ignore base ID(base ID translation - Work in progress)***

## Build instructions

* Windows [(32-bit)][win32] [(64-bit)][win64]
* [macOS][mac]
* [GNU/Linux using Docker][linux]

## Links

* [Official Telegram Channel](https://t.me/tg_x64)
* [Official discussion group](https://t.me/tg_x64_chat)

## Sponsors
<a href="https://www.jetbrains.com/?from=64Gram">
     <img src="https://www.jetbrains.com/icon-512.png"  alt="JetBrains" width="150"/>
</a>

[//]: # (LINKS)
[license]: LICENSE
[win32]: docs/building-win.md
[win64]: docs/building-win-x64.md
[mac]: docs/building-mac.md
[linux]: docs/building-linux.md
[preview_image]: https://github.com/TDesktop-x64/tdesktop/blob/dev/docs/assets/preview.png "Preview of 64Gram Desktop"
[preview_image_url]: https://raw.githubusercontent.com/TDesktop-x64/tdesktop/dev/docs/assets/preview.png
