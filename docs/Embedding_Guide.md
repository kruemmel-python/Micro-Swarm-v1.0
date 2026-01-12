# Embedding Guide (Python / Rust / Unity / Unreal)

This guide explains how to embed the Micro-Swarm DLL in common environments.

## Python (ctypes)

- Use `ct.CDLL` on Windows (cdecl).
- Place `micro_swarm.dll` next to your script or add to PATH.
- Use the examples in `DLL_Nutzung.md`.

## Rust (FFI)

- Use `extern "C"` with `#[repr(C)]` structs.
- Link by placing the DLL in the executable folder or via PATH.
- Use the minimal example in `DLL_Nutzung.md` and add a safe wrapper with `Drop`.

## Unity (C#)

1) Place `micro_swarm.dll` in `Assets/Plugins/`.
2) Define P/Invoke signatures:
   - Use `[DllImport("micro_swarm", CallingConvention = CallingConvention.Cdecl)]`.
3) Use `IntPtr` for handles and marshal `float[]` for fields.

Minimal C# snippet:
```csharp
using System;
using System.Runtime.InteropServices;

public static class MicroSwarm {
    [DllImport("micro_swarm", CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr ms_create(IntPtr cfg);

    [DllImport("micro_swarm", CallingConvention = CallingConvention.Cdecl)]
    public static extern void ms_destroy(IntPtr h);
}
```

## Unreal (C++)

1) Add DLL to your project `Binaries/Win64/`.
2) Load with `FPlatformProcess::GetDllHandle`.
3) Bind functions via `FPlatformProcess::GetDllExport`.
4) Ensure calling convention is cdecl.

Minimal Unreal snippet:
```cpp
void* Lib = FPlatformProcess::GetDllHandle(TEXT("micro_swarm.dll"));
auto MsCreate = (ms_handle_t*(*)(const ms_config_t*))FPlatformProcess::GetDllExport(Lib, TEXT("ms_create"));
```

## Notes

- Always call `ms_destroy`.
- `ms_copy_field_in/out` expects float count (not bytes).
- For GPU, call `ms_ocl_enable` after `ms_create`.
- MycoDB API is available via `ms_db_*` functions; see `examples/MycoDbExample.cs`.
