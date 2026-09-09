# TwinVQ / VQF decoder for foobar2000

Independent TwinVQ decoder for Windows. It plays NTT / Yamaha SoundVQ files (`.vqf`, `.vql`, `.vqe`) in [foobar2000](https://www.foobar2000.org/) and can decode them to WAV from the command line.

The codec implementation does **not** use FFmpeg, `tvqdec.dll`, or the Yamaha SoundVQ SDK.

## Features

- Decode TwinVQ / VQF (the proprietary NTT bitstream, not MPEG-4 TwinVQ)
- Modes from 8 kHz / 8 kbit/s/ch up to 44.1 kHz / 48 kbit/s/ch
- Read and write VQF metadata (`NAME`, `AUTH`, `COMT`, `(c) `, `ALBM`, `GENR`, `TRCK`, `YEAR`, `MUSC`, `LABL`)
- Seeking (constant bitrate)
- CLI decoder `vqf_decode.exe`

## Layout

```
twinvq/            TwinVQ library (no foobar2000 dependency)
  include/twinvq/  Public headers
  src/             Decoder, parser, IMDCT, codebooks
  tables/          Optional codebook generator
foo_input_vqf/     foobar2000 input component
tools/             vqf_decode CLI
audio/             Sample .vqf files (see audio/README.md)
sdk/               foobar2000 SDK — not in git; you add this locally
```

The foobar2000 SDK is **not** part of this repository.

## Requirements

- Windows x64
- Visual Studio 2022 or later, with the C++ x64 toolset and a Windows 10/11 SDK
- [foobar2000](https://www.foobar2000.org/) 2.x (x64) to load the component
- [foobar2000 SDK](https://www.foobar2000.org/SDK) **only** if you build `foo_input_vqf`

The `twinvq` library and `vqf_decode` CLI build without the SDK.

## foobar2000 SDK (component build)

1. Download the SDK from <https://www.foobar2000.org/SDK>.
2. Extract it so the tree looks like this:

```
sdk/
  foobar2000/
    SDK/
    shared/
    foobar2000_component_client/
  pfc/
```

The official zip is named `SDK-YYYY-MM-DD`. Either rename that folder to `sdk`, or extract its contents into `sdk/`.

You can also point MSBuild at another location:

```
msbuild vqf_decoder.sln /p:FOOBAR2000_SDK=C:\path\to\SDK-2025-03-07 ...
```

## Build

From the repository root, in a Visual Studio developer prompt:

```bat
msbuild vqf_decoder.sln /p:Configuration=Release /p:Platform=x64
```

If the SDK projects still ask for toolset v142, add one of:

```bat
rem Visual Studio 2022
msbuild vqf_decoder.sln /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143

rem Visual Studio 2026
msbuild vqf_decoder.sln /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145
```

Outputs:

| File | Purpose |
| --- | --- |
| `bin\x64\Release\vqf_decode.exe` | Command-line decoder |
| `bin\x64\Release\foo_input_vqf.dll` | foobar2000 component |

Decoder-only (no SDK):

```bat
msbuild twinvq\twinvq.vcxproj /p:Configuration=Release /p:Platform=x64
msbuild tools\vqf_decode.vcxproj /p:Configuration=Release /p:Platform=x64
```

## Install the foobar2000 component

1. Copy `foo_input_vqf.dll` to:

   `%APPDATA%\foobar2000-v2\user-components-x64\foo_input_vqf\foo_input_vqf.dll`

2. Restart foobar2000.
3. Check **File → Preferences → Components** for “TwinVQ decoder”.

## Command line

```bat
bin\x64\Release\vqf_decode.exe audio\koshuks-40kb.vqf out.wav
bin\x64\Release\vqf_decode.exe --test-tags audio\koshuks-40kb.vqf
```

## Sample files

`audio/` contains public TwinVQ samples of *(Walk Among) Koshuks*, downloaded from <http://www.onlovestar.com/noise/vqf.htm>. See [audio/README.md](audio/README.md).

## License

This project’s original source is MIT. See [LICENSE](LICENSE).

The foobar2000 SDK (not included) is copyright Peter Pawlowski and is licensed separately.

Codebook tables are numerical TwinVQ constants required to decode the format. `twinvq/tables/gen_tables.py` can regenerate `twinvq_tables.cpp` from publicly documented table data.

Known NTT TwinVQ patent families expired around 2015–2020. That is not legal advice.
