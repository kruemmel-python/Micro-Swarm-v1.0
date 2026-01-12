# Unreal Embedding Guide

This guide shows how to embed Micro-Swarm in Unreal and what is realistic.

## What is possible

- Run headless simulations inside tools or editor utilities.
- Pull simulation fields for visualization or gameplay logic.
- Push custom fields for interaction or scripted perturbations.
- Deterministic runs via seed and explicit stepping.

## Setup

1) Copy `micro_swarm.dll` into your project's `Binaries/Win64/`.
2) Ensure the DLL is built for x64.
3) Exports use cdecl (default C calling convention).

## Minimal C++ Example (Correct ABI)

```cpp
#include "HAL/PlatformProcess.h"

using ms_handle_t = void*;
using ms_create_fn = ms_handle_t (*)(const void* cfg);
using ms_destroy_fn = void (*)(ms_handle_t h);
using ms_step_fn = int (*)(ms_handle_t h, int steps);

void RunSwarm() {
    void* Lib = FPlatformProcess::GetDllHandle(TEXT("micro_swarm.dll"));
    if (!Lib) return;

    auto MsCreate = (ms_create_fn)FPlatformProcess::GetDllExport(Lib, TEXT("ms_create"));
    auto MsDestroy = (ms_destroy_fn)FPlatformProcess::GetDllExport(Lib, TEXT("ms_destroy"));
    auto MsStep = (ms_step_fn)FPlatformProcess::GetDllExport(Lib, TEXT("ms_step"));
    if (!MsCreate || !MsDestroy || !MsStep) {
        FPlatformProcess::FreeDllHandle(Lib);
        return;
    }

    // Recommended: pass a real config struct from micro_swarm_api.h.
    // ms_create(nullptr) only works with defaults and limits control.
    ms_handle_t ctx = MsCreate(nullptr);
    if (!ctx) {
        FPlatformProcess::FreeDllHandle(Lib);
        return;
    }

    int rc = MsStep(ctx, 10);
    if (rc <= 0) {
        MsDestroy(ctx);
        FPlatformProcess::FreeDllHandle(Lib);
        return;
    }

    MsDestroy(ctx);
    FPlatformProcess::FreeDllHandle(Lib);
}
```

## Recommended Integration (Full Control)

- Include `micro_swarm_api.h` in a plugin module and call `ms_create(&cfg)`.
- Initialize `ms_config_t` with width/height/agent_count/steps and a seed.
- Use `ms_get_field_info` + `ms_copy_field_out` to drive textures or buffers.
- Use `ms_copy_field_in` for gameplay interaction (e.g., set danger zones).

## What you can build

- Niagara: density/color driven by field heatmaps.
- Materials: resource or pheromone fields as world-space overlays.
- Tools: editor widgets for batch runs and report generation.
- Deterministic replays using the same seed and parameters.

## Safety Notes

- A context must not be used concurrently from multiple threads.
- `ms_copy_field_*` counts are number of floats, not bytes.
- Use `ms_get_api_version` to verify DLL/header compatibility.
