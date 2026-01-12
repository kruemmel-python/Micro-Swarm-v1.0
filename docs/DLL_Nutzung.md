# Nutzung der Micro-Swarm DLL

Die DLL stellt eine vollstaendige C-API bereit (FFI-sicher) und kann aus Python, Rust oder anderen Sprachen aufgerufen werden.

Dateien:
- `micro_swarm.dll` (Windows) bzw. `libmicro_swarm.so` (Linux)
- `src/micro_swarm_api.h` (C-API Header)

## Wichtiges Grundprinzip

- Alle Funktionen sind synchron, kein internes Threading.
- Alle Structs sind POD und `repr(C)` kompatibel.
- Felder werden als `float*` im Row-Major-Format genutzt (`width * height`).
- Ownership: `ms_create()` liefert einen Handle, der mit `ms_destroy()` freigegeben wird.
- Calling Convention (Windows): **__cdecl** (Standard-C). In Python daher `ct.CDLL` verwenden.
- Packing: Es wird **kein** spezielles `#pragma pack` verwendet (Default-Alignment).
- Thread-Safety: Ein Kontext ist **nicht** thread-safe; parallele Calls auf denselben Handle sind undefiniert.
- Thread-Ownership: Ein Kontext darf von genau einem Thread gleichzeitig benutzt werden; Uebergabe zwischen Threads ist erlaubt, parallele Nutzung nicht.

### Rueckgabewerte / Fehler

- Query/Lookup-Funktionen liefern die Anzahl Treffer; **0** ist dabei ein valides Ergebnis (z. B. keine Treffer).
- Action/Lifecycle-Funktionen mit `int`-Rueckgabe liefern **0 bei Fehler**, **>0 bei Erfolg**.
- `ms_step/ms_run` liefern die ausgefuehrten Schritte (0 bei Fehler/ungueligem Handle).
- `ms_copy_field_in/out` erwarten `count` als **Anzahl Floats** (nicht Bytes).
- `ms_get_field_info` gibt `w/hgt` aus, hat **keinen** Returncode.
- Bei MycoDB liefert `ms_db_get_last_error()` den letzten Fehlertext fuer diesen DB-Handle.

Funktionsklassen (Kurzregel):
- Query/Lookup: Rueckgabe = Trefferanzahl (0 = ok, "no results")
- Copy/Write/Step/Run: Rueckgabe = Anzahl geschriebener Elemente / Schritte (0 = Fehler)
- Create: Rueckgabe = Handle (NULL = Fehler), nicht int
- Getter ohne Returncode: nur Out-Parameter gueltig, wenn Handle gueltig

Diese Klassifizierung ist Teil des API-Vertrags.

#### Returncode-Tabelle (aktuell)

| Code | Bedeutung |
|------|-----------|
| > 0  | Erfolg (z. B. Anzahl geschriebener Elemente oder Schritte) |
| 0    | Fehler (Action/Lifecycle) **oder** 0 Treffer (Query/Lookup) |

### Struct-Groesse / ABI-Hinweis

- Die Struct-Groessen muessen mit dem verwendeten `micro_swarm_api.h` uebereinstimmen.
- Kein `struct_size` Feld; Structs sind nicht versionstolerant.
- Header und DLL muessen exakt zusammenpassen; andernfalls ist Verhalten undefiniert. Dynamische Loader muessen `ms_get_api_version` pruefen.

### Field-IDs (Enum)

```
0 = RESOURCES
1 = PHEROMONE_FOOD
2 = PHEROMONE_DANGER
3 = MOLECULES
4 = MYCEL
```

### DLL-Suche (Windows)

Die DLL muss sich im Arbeitsverzeichnis, neben dem Python-Script oder im `PATH` befinden.

### OpenCL-Hinweis

OpenCL muss auf dem Zielsystem installiert/verfuegbar sein, wenn GPU-Funktionen genutzt werden. Ohne OpenCL faellt die DLL automatisch auf CPU zurueck.

### Lifecycle-Hinweis

`ms_get_field_info`, `ms_copy_field_out` und `ms_copy_field_in` sind nur zwischen `ms_create` und `ms_destroy` gueltig.

---

## MycoDB (Datenbank) API

Die MycoDB-Funktionen sind in der DLL verfuegbar und nutzen einen separaten Handle:

- `ms_db_create()` / `ms_db_destroy()`
- `ms_db_load_sql()` / `ms_db_run_ingest()` / `ms_db_save_myco()` / `ms_db_load_myco()`
- `ms_db_query_sql()` / `ms_db_query_simple()` / `ms_db_query_by_id()`
- Fokus-Varianten: `ms_db_query_simple_focus()` / `ms_db_query_by_id_focus()`
- Ergebniszugriff: `ms_db_get_result_count()`, `ms_db_get_result_indices()`, `ms_db_get_payload()`, `ms_db_get_payload_raw()`
- Hilfen: `ms_db_find_payload_by_id()`, `ms_db_get_payload_count()`, `ms_db_get_table_count()`

Fehlertexte koennen ueber `ms_db_get_last_error()` abgefragt werden.
Swarm-Handle (`ms_handle_t`) und DB-Handle (`ms_db_handle_t`) sind nicht kompatibel und duerfen nicht gemischt werden.

Ein komplettes C# Beispiel findest du in `examples/MycoDbExample.cs`.

---

## Python (ctypes) Beispiel (robust)

```python
import ctypes as ct
from pathlib import Path

DLL_PATH = Path(r".\build\Release\micro_swarm.dll")

# WICHTIG:
# - __cdecl: ct.CDLL (Standard-C)
# - __stdcall: ct.WinDLL
lib = ct.CDLL(str(DLL_PATH))

class MicroSwarmError(RuntimeError):
    pass

def _check_rc(rc: int, fn_name: str) -> int:
    if rc <= 0:
        raise MicroSwarmError(f"{fn_name} failed with rc={rc}")
    return rc

class ms_params_t(ct.Structure):
    _fields_ = [
        ("width", ct.c_int),
        ("height", ct.c_int),
        ("agent_count", ct.c_int),
        ("steps", ct.c_int),
        ("pheromone_evaporation", ct.c_float),
        ("pheromone_diffusion", ct.c_float),
        ("molecule_evaporation", ct.c_float),
        ("molecule_diffusion", ct.c_float),
        ("resource_regen", ct.c_float),
        ("resource_max", ct.c_float),
        ("mycel_decay", ct.c_float),
        ("mycel_growth", ct.c_float),
        ("mycel_transport", ct.c_float),
        ("mycel_drive_threshold", ct.c_float),
        ("mycel_drive_p", ct.c_float),
        ("mycel_drive_r", ct.c_float),
        ("agent_move_cost", ct.c_float),
        ("agent_harvest", ct.c_float),
        ("agent_deposit_scale", ct.c_float),
        ("agent_sense_radius", ct.c_float),
        ("agent_random_turn", ct.c_float),
        ("dna_capacity", ct.c_int),
        ("dna_global_capacity", ct.c_int),
        ("dna_survival_bias", ct.c_float),
        ("phero_food_deposit_scale", ct.c_float),
        ("phero_danger_deposit_scale", ct.c_float),
        ("danger_delta_threshold", ct.c_float),
        ("danger_bounce_deposit", ct.c_float),
        ("evo_enable", ct.c_int),
        ("evo_elite_frac", ct.c_float),
        ("evo_min_energy_to_store", ct.c_float),
        ("evo_mutation_sigma", ct.c_float),
        ("evo_exploration_delta", ct.c_float),
        ("evo_fitness_window", ct.c_int),
        ("evo_age_decay", ct.c_float),
        ("global_spawn_frac", ct.c_float),
    ]

class ms_config_t(ct.Structure):
    _fields_ = [("params", ms_params_t), ("seed", ct.c_uint32)]

ms_handle_t = ct.c_void_p

lib.ms_create.argtypes = [ct.POINTER(ms_config_t)]
lib.ms_create.restype = ms_handle_t

lib.ms_destroy.argtypes = [ms_handle_t]
lib.ms_destroy.restype = None

lib.ms_step.argtypes = [ms_handle_t, ct.c_int]
lib.ms_step.restype = ct.c_int

lib.ms_get_field_info.argtypes = [ms_handle_t, ct.c_int, ct.POINTER(ct.c_int), ct.POINTER(ct.c_int)]
lib.ms_get_field_info.restype = None

lib.ms_copy_field_out.argtypes = [ms_handle_t, ct.c_int, ct.POINTER(ct.c_float), ct.c_int]
lib.ms_copy_field_out.restype = ct.c_int

cfg = ms_config_t()
cfg.params.width = 128
cfg.params.height = 128
cfg.params.agent_count = 512
cfg.params.steps = 100
cfg.seed = 42

ctx = lib.ms_create(ct.byref(cfg))
if not ctx:
    raise MicroSwarmError("ms_create returned NULL")

rc = lib.ms_step(ctx, 10)
_check_rc(rc, "ms_step")

w = ct.c_int()
hgt = ct.c_int()
MS_FIELD_PHEROMONE_FOOD = 1
lib.ms_get_field_info(ctx, MS_FIELD_PHEROMONE_FOOD, ct.byref(w), ct.byref(hgt))
count = w.value * hgt.value
buffer = (ct.c_float * count)()
rc = lib.ms_copy_field_out(ctx, MS_FIELD_PHEROMONE_FOOD, buffer, count)
_check_rc(rc, "ms_copy_field_out")

lib.ms_destroy(ctx)
```

---

## Rust (FFI) Beispiel

```rust
#[repr(C)]
pub struct ms_params_t {
    width: i32,
    height: i32,
    agent_count: i32,
    steps: i32,
    pheromone_evaporation: f32,
    pheromone_diffusion: f32,
    molecule_evaporation: f32,
    molecule_diffusion: f32,
    resource_regen: f32,
    resource_max: f32,
    mycel_decay: f32,
    mycel_growth: f32,
    mycel_transport: f32,
    mycel_drive_threshold: f32,
    mycel_drive_p: f32,
    mycel_drive_r: f32,
    agent_move_cost: f32,
    agent_harvest: f32,
    agent_deposit_scale: f32,
    agent_sense_radius: f32,
    agent_random_turn: f32,
    dna_capacity: i32,
    dna_global_capacity: i32,
    dna_survival_bias: f32,
    phero_food_deposit_scale: f32,
    phero_danger_deposit_scale: f32,
    danger_delta_threshold: f32,
    danger_bounce_deposit: f32,
    evo_enable: i32,
    evo_elite_frac: f32,
    evo_min_energy_to_store: f32,
    evo_mutation_sigma: f32,
    evo_exploration_delta: f32,
    evo_fitness_window: i32,
    evo_age_decay: f32,
    global_spawn_frac: f32,
}

#[repr(C)]
pub struct ms_config_t {
    params: ms_params_t,
    seed: u32,
}

extern "C" {
    fn ms_create(cfg: *const ms_config_t) -> *mut std::ffi::c_void;
    fn ms_destroy(h: *mut std::ffi::c_void);
    fn ms_step(h: *mut std::ffi::c_void, steps: i32) -> i32;
}

fn main() {
    let mut cfg = ms_config_t { params: unsafe { std::mem::zeroed() }, seed: 42 };
    cfg.params.width = 128;
    cfg.params.height = 128;
    cfg.params.agent_count = 512;
    cfg.params.steps = 100;
    unsafe {
        let h = ms_create(&cfg);
        if h.is_null() {
            panic!("ms_create returned NULL");
        }
        let rc = ms_step(h, 10);
        if rc <= 0 {
            ms_destroy(h);
            panic!("ms_step failed rc={rc}");
        }
        ms_destroy(h);
    }
}
```

---

## C Minimalbeispiel (ABI Ground Truth)

```c
#include "micro_swarm_api.h"

int main(void) {
    ms_config_t cfg = {0};
    cfg.params.width = 128;
    cfg.params.height = 128;
    cfg.params.agent_count = 512;
    cfg.seed = 42;

    ms_handle_t *h = ms_create(&cfg);
    if (!h) return 1;
    if (ms_step(h, 10) <= 0) return 2;
    ms_destroy(h);
    return 0;
}
```

---

## Ownership / Hinweise

- `ms_create()` erzeugt den Kontext und initialisiert Felder + Agenten.
- `ms_destroy()` MUSS immer aufgerufen werden.
- `ms_copy_field_in/out` arbeitet mit rohen Float-Arrays.
- `ms_ocl_enable()` kann die GPU-Diffusion aktivieren (falls OpenCL vorhanden).
