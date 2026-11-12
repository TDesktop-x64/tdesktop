# V1-style zstd update packages

`Packer_zstd` packages stable and beta updates on Windows, macOS and other
Unix platforms using the v1 layout with zstd instead of LZMA. The original
`Packer` sources are retained, but its CMake target is no longer generated.
Only `Packer_zstd` appears in the solution and is built with the application. The standalone `Updater` installs
files prepared by either decoder and handles the executable-name migration.
The client dispatches zstd packages separately and retains its original v2 path.
Users with the already-shipped v2-only client must install the first build
supporting zstd manually.

## Packing

Use the same arguments, changing only the executable name:

```bat
set PackVersion=1002012
Packer_zstd.exe -path 64Gram.exe -path Updater.exe -path "modules\x64\d3d\d3dcompiler_47.dll" -version %PackVersion%
```

This produces `tupdate1002012`, keeping the original Packer filename.
Platform selection follows v1:

| Platform / option | Output |
| --- | --- |
| Windows, no target or `-target win` | `tupdate1002012` |
| Windows, `-target win64` | `tx64upd1002012` |
| Windows, `-target winarm` | `tarm64upd1002012` |
| macOS, no arch or `-arch x86_64` | `tmacupd1002012` |
| macOS, `-arch arm64` | `tarmacupd1002012` |
| Other Unix platforms | `tlinuxupd1002012` |

Add `-beta` to use the existing beta key; the filename stays the same as v1.
Repeated `-path` arguments include files or directories. Directory inputs
retain their directory name, except `64Gram.app` is stored as `Telegram.app`
so existing macOS clients can find the packaged Updater. The new helper installs
it as `64Gram.app` and removes the previous bundle contents. All inputs must
be under the first input's parent directory. On macOS pass the app bundle;
on Unix pass `64Gram` and
`Updater` along with runtime files required by your release.

The default compression level is 17, with one worker per logical CPU
reported by the machine (falling back to one if detection fails). Override
with `-compression-level 1..19` and `-threads <positive count>`.
Existing output files are never overwritten.

Publish the package and point the existing stable/beta HTTP feed at its
original-style filename with the matching numeric application version.
Architecture and channel selection remain the feed's responsibility, as in
v1; the package has no target or channel field. Existing build/publication
scripts continue to invoke the original Packer unless explicitly changed.

## Keys and build

Packer_zstd automatically signs with `PrivateKey` or `PrivateBetaKey` from
`../DesktopPrivate/packer_private.h`, using the existing v1 RSA-1024 keys.
The corresponding public keys live in `SourceFiles/core/update_zstd.cpp`.
It verifies its own signature and checks decompression before writing output.
There is no manifest or separate signing step for this format.

For disposable tests, `-private-key` and `-public-key` accept PEM files as a
pair; clients still trust only their compiled-in keys. Public keys use PKCS#1
`RSA PUBLIC KEY` encoding. Defining `PACKER_DISABLE_PRIVATE` allows compiling
the new tool without the private header. Keep production private keys out
of this repository.

CMake adds the `Packer_zstd` target alongside `Packer` for special-target
builds. Bundled builds fetch pinned zstd 1.5.7 with multithreading; an offline
build may set `FETCHCONTENT_SOURCE_DIR_UPDATE_ZSTD` to verified source.
Packaged builds prefer `libzstd >= 1.5.0` via pkg-config and fall back to
the same pinned source when the package is unavailable.

## Wire format

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 128 | RSA PKCS#1 v1.5 SHA-1 signature of the stored digest |
| 128 | 20 | SHA-1 digest of all bytes starting at offset 148 |
| 148 | 4 | Uncompressed file-list size, little-endian |
| 152 | Remaining bytes | One zstd frame with content size and checksum |

This retains v1's signature, digest and size fields. Zstd replaces the
platform-dependent LZMA payload and requires no LZMA properties. It is a new
format, identified by the zstd frame magic inside the package; old
v1 clients cannot decompress it.

The file list retains `QDataStream::Qt_5_1` serialization: quint32 version,
quint32 count, then each QString relative path, quint32 file size and
QByteArray content. Non-Windows packages append a boolean executable flag
for each file. No canary/alpha encoding is added.

Verification accepts the existing stable or beta key, authenticates before
decompression, and rejects equal/older versions. Limits are 256 MiB per
package and 1 GiB uncompressed. The decoder requires exactly one complete
frame, a matching known content size and no external dictionary. Extraction
reuses the existing filename, duplicate, size and protected-install checks.

The application binaries are now `64Gram.exe`, `64Gram.app`, and `64Gram`.
The CMake target remains `Telegram`, and update archive filenames are unchanged.
The helpers migrate the old executable after copying the new files. Windows
repairs matching shortcuts and registered protocol paths before cleanup. Linux
uses a temporary relaunch symlink for older clients, removed by the new app.
Clients that cannot read zstd still need a manual installation. Old protected
Linux installers that require a `Telegram` payload also need a manual upgrade. User-data paths and application IDs are unchanged.
