# Quick Menu Plus

Quick Menu Plus adds the following features to the Quick Menu on the PlayStation Vita and PlayStation TV.

- Power off and standby buttons for Vita
- Restart by holding the power off button
- Volume slidebar for Vita
- Configurable Quick Menu PS button push time

![preview.png](https://git.shotatoshounenwachigau.moe/vita/quickmenuplus/plain/preview-small.png?h=assets)

## Installation

Supported firmware versions:

- Retail 3.60-3.73
- Testkit 3.60, 3.65
- Devkit 3.60

Install under `*main` of your taiHEN config.

```
*main
ur0:/tai/quickmenuplus.suprx
```

## Configuration

Put files in the directory `ur0:/data/quickmenuplus` to configure Quick Menu Plus.

- `pushtime.txt` sets the Quick Menu PS button push time. Put in the file the desired push time in microseconds. For example, put `250000` for 250 milliseconds. If the file does not exist, the push time is unchanged.

## Contributing

Use [git-format-patch](https://www.git-scm.com/docs/git-format-patch) or [git-request-pull](https://www.git-scm.com/docs/git-request-pull) and email me at <asakurareiko@protonmail.ch>.

## See also

- [Discussion](https://forum.devchroma.nl/index.php/topic,78.0.html)
- [Source code](https://git.shotatoshounenwachigau.moe/vita/quickmenuplus)

## Credits

- Princess-of-Sleeping: Tip for SceShellUtil, ScePaf
