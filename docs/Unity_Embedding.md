# Unity Embedding Guide

This guide shows how to use Micro-Swarm in Unity (C#) and what is possible.

## Was moeglich ist (realistisch & sinnvoll)

- Headless-Simulation aus C# (synchron).
- Felder ziehen (`ms_copy_field_out`) und als `Texture2D`/`ComputeBuffer` visualisieren.
- Felder schieben (`ms_copy_field_in`) fuer Interaktion (Gameplay/Editor-Tools).
- Determinismus ueber Seed (Replays, Vergleichslaeufe).
- Optional GPU-Diffusion per OpenCL (falls `ms_ocl_enable` genutzt wird und Runtime vorhanden ist).

## Setup

1) `micro_swarm.dll` nach `Assets/Plugins/x86_64/` (empfohlen) oder `Assets/Plugins/`.
2) In Unity im Inspector der DLL:
   - CPU: x86_64
   - OS: Windows
   - Load on startup: optional
3) Calling Convention: `Cdecl`.

Wichtig: Unity laedt native DLLs je nach Editor/Player-Kontext anders. Fuer Builds ist `Assets/Plugins/x86_64/` die sauberste Loesung.

## Minimal, aber korrekt: Create -> Step -> Destroy

```csharp
using System;
using System.Runtime.InteropServices;

public static class MicroSwarm {
    [DllImport("micro_swarm", CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr ms_create(ref ms_config_t cfg);

    [DllImport("micro_swarm", CallingConvention = CallingConvention.Cdecl)]
    public static extern void ms_destroy(IntPtr h);

    [DllImport("micro_swarm", CallingConvention = CallingConvention.Cdecl)]
    public static extern int ms_step(IntPtr h, int steps);
    [StructLayout(LayoutKind.Sequential)]
    public struct ms_params_t {
        public int width;
        public int height;
        public int agent_count;
        public int steps;
        public float pheromone_evaporation;
        public float pheromone_diffusion;
        public float molecule_evaporation;
        public float molecule_diffusion;
        public float resource_regen;
        public float resource_max;
        public float mycel_decay;
        public float mycel_growth;
        public float mycel_transport;
        public float mycel_drive_threshold;
        public float mycel_drive_p;
        public float mycel_drive_r;
        public float agent_move_cost;
        public float agent_harvest;
        public float agent_deposit_scale;
        public float agent_sense_radius;
        public float agent_random_turn;
        public int dna_capacity;
        public int dna_global_capacity;
        public float dna_survival_bias;
        public float phero_food_deposit_scale;
        public float phero_danger_deposit_scale;
        public float danger_delta_threshold;
        public float danger_bounce_deposit;
        public int evo_enable;
        public float evo_elite_frac;
        public float evo_min_energy_to_store;
        public float evo_mutation_sigma;
        public float evo_exploration_delta;
        public int evo_fitness_window;
        public float evo_age_decay;
        public float global_spawn_frac;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ms_config_t {
        public ms_params_t params;
        public uint seed;
    }
}

public sealed class SwarmRunner : IDisposable {
    private IntPtr _ctx;

    public SwarmRunner(int w, int h, int agents, int steps, uint seed) {
        var cfg = new MicroSwarm.ms_config_t {
            params = new MicroSwarm.ms_params_t {
                width = w,
                height = h,
                agent_count = agents,
                steps = steps
            },
            seed = seed
        };
        _ctx = MicroSwarm.ms_create(ref cfg);
        if (_ctx == IntPtr.Zero) throw new Exception("ms_create returned NULL");
    }

    public void Step(int n) {
        int rc = MicroSwarm.ms_step(_ctx, n);
        if (rc <= 0) throw new Exception($"ms_step failed rc={rc}");
    }

    public void Dispose() {
        if (_ctx != IntPtr.Zero) {
            MicroSwarm.ms_destroy(_ctx);
            _ctx = IntPtr.Zero;
        }
    }
}
```

## Was man damit konkret machen kann

- Heatmap-Textures: Feld in `Texture2D` (RFloat/RGBA32 normalisiert).
- GPU-Visualisierung: Feld als `ComputeBuffer` -> `ComputeShader`.
- Interaktion: Maus/Collider -> lokal ins Feld schreiben -> `ms_copy_field_in`.
- Editor Tools: Headless Batch Runs + automatischer Export.

## Hinweise

- Struct-Reihenfolge exakt wie in `micro_swarm_api.h` halten.
- `ms_copy_field_*`: `count` = Anzahl Floats, nicht Bytes.
- `ms_get_api_version` kann genutzt werden, um DLL/Header zu pruefen.
