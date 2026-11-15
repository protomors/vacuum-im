# Third-party sources

`VACUUM_DEPENDENCY_PROVIDER=SYSTEM` uses installed development packages.
`BUNDLED` builds the snapshots below as static libraries; `AUTO` prefers system
packages and uses these snapshots when a development package is absent.
The build does not download dependencies at configure time.

| Library | Bundled release | Source archive |
| --- | --- | --- |
| zlib | 1.4 | https://zlib.net/zlib-1.4.tar.gz |
| MiniZip (classic API) | zlib 1.4 contrib/minizip | Same archive as zlib |
| Hunspell | 1.7.6 | https://github.com/hunspell/hunspell/releases/download/v1.7.6/hunspell-1.7.6.tar.gz |
| GNU libidn Stringprep | 1.44 | https://ftp.gnu.org/gnu/libidn/libidn-1.44.tar.gz |

SHA-256 of the source archives:

```
zlib:     02dd159783677a749c04adbdd02b55b8a2bcc7e054f51b76b549199a916ad584
hunspell: 9c650ace31d4d8cc1da925a9507c3de1d50f1a54857af2f72429f517d5d53467
libidn:   499608bab3a65650a0ea52888c13a8deebe3f71408e319acd9ec52e02eb13959
```

The upstream source files are unmodified. Local CMake files select the library
sources, export include paths/definitions through targets, enable PIC, and avoid
installing third-party SDKs or command-line tools into the application archive.
When refreshing a snapshot, replace its upstream files and retain those CMake
integration files. Each directory contains its upstream license texts; MiniZip
uses the zlib license and the notices in its source files.

* zlib contains the root C sources and headers, README, ChangeLog and LICENSE.
* MiniZip contains contrib/minizip's library C sources and headers, excluding
  miniunz.c and minizip.c, plus MiniZip64_info.txt as README. The Windows file
  implementation is compiled only on Windows.
* Hunspell contains src/hunspell's C++ sources and headers, plus its README and
  license texts. utf_info.hxx is included by csutil.cxx rather than compiled.
* libidn contains lib/{stringprep,profiles,rfc3454,nfkc}.c, stringprep.h,
  rfc3454.h, gunicomp.h and gunidecomp.h, and lib/gl/unistr/u8-check.c. This is
  the Stringprep/NFKC subset used for XMPP JIDs, not libidn2 (which is not a
  drop-in replacement for the XMPP profiles). The local idn-int.h uses standard
  integer types and supplies ssize_t for MSVC. support/config.h and
  support/unistr.h provide only the configuration/declaration needed by this
  subset, avoiding a full gnulib/libunistring dependency.

QtLockedFile, QxtGlobalShortcut and Idle are locally maintained Qt helpers.
They are always compiled against the application's Qt version.
