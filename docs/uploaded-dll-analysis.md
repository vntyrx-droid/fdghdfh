# What the uploaded `Chorus_x64.dll` actually was

Static analysis only — headers, export tables and embedded strings. The binary
was never decompiled, and no code from it was used in this repository.

## Summary

The file is **Fruity Chorus**, Image-Line's stock FL Studio effect, taken out of
an FL Studio installation. It is not a malformed VST. It is a *native FL Studio
plugin*, a format that only FL Studio loads, which is why every other host
rejected it.

## Evidence

### PE header

```
Format            PE32+ (x86-64) DLL, GUI subsystem, 10 sections
Image base        0x0000000000400000
Timestamp         2025-02-27
Security dir      present (Authenticode signed)
```

### Export table

```
ord 3   CreatePlugInstance
ord 2   __dbk_fcall_wrapper
ord 1   dbkFCallWrapperAddr
```

`CreatePlugInstance` is the FL Plugin SDK entry point. Its SDK signature is:

```cpp
extern "C" __declspec(dllexport)
TFruityPlug* _stdcall CreatePlugInstance (TFruityPlugHost* host, int tag);
```

The other two are Borland/Embarcadero debug-kernel exports, present in most
Delphi-built binaries.

### Internal name

The export directory names the module `FruityPlug_x64.dll`, not `Chorus_x64.dll`
— the file was renamed on disk at some point.

### Embedded strings

```
Fruity Chorus
TFruityPlug
TDelphiFruityPlug
FP_DelphiPlug
TList<FP_DelphiPlug.TRegisteredSideIO>
FL Studio theme file (*.xml)|*.xml|Zipped theme file (*.zip)|*.zip
Embarcadero Delphi for Win64 compiler version 36.0 (29.0.52161.7750)
```

Delphi emits rich RTTI, which is why the class names survive in the binary.

### Imports

Only Win32 system libraries — `kernel32`, `user32`, `gdi32`, `ole32`,
`oleaut32`, `comctl32`, `shell32`, `advapi32`, `winmm`, `msacm32`, `version`.
No VST, CLAP or audio-framework dependency of any kind.

## Why no host would load it

A host decides whether a DLL is a plugin by looking for a known exported entry
point:

| Format | Required export | Present? |
| --- | --- | --- |
| VST2 | `VSTPluginMain` (or `main`) | no |
| VST3 | `GetPluginFactory` | no |
| CLAP | `clap_entry` | no |
| FL native | `CreatePlugInstance` | **yes** |

The DLL exports only the FL native entry point, so every VST/CLAP host correctly
concludes it is not a plugin they can load. Nothing is corrupted; the file is
simply being offered to the wrong kind of host.

Even in FL Studio, stock plugins are discovered by their install layout rather
than by a generic plugin scan — roughly:

```
...\Image-Line\FL Studio 20xx\Plugins\Fruity\Effects\Chorus\Chorus_x64.dll
```

A copy sitting in a VST folder will not be picked up.

## Why it was not decompiled

It is signed, closed-source, commercial software owned by Image-Line, not the
user's own work, so turning it into rebuildable source and shipping a derivative
is not something this project does.

Setting licensing aside, the technical return would have been poor. Delphi
binaries give up class names through RTTI but little else: the DSP is inlined,
scheduled SSE arithmetic with no symbols, no types and no structure. The output
would be tens of thousands of lines of unmaintainable pseudo-C — strictly worse
than the few hundred lines of clear code in this repository.

## The legitimate routes

1. **Write an original plugin.** What this repository does. A chorus is
   well-understood DSP — modulated fractional delay lines, LFOs, a wet/dry
   blend — and the result is yours outright.
2. **Build a real FL native plugin.** Image-Line publishes the FL Plugin SDK
   for free; it supports Delphi and Visual C++ and is distributed through
   Image-Line's [developer forum](https://forum.image-line.com/viewtopic.php?t=12092).
   Use it if you specifically want FL-native integration rather than a VST3.
3. **Use the stock plugin as intended.** If the goal was just to *use* Fruity
   Chorus, it ships with FL Studio and works there already.
