using System;
using System.Runtime.InteropServices;
using UnityEngine;

public class MicroSwarmRunner : MonoBehaviour, IDisposable
{
    public enum FieldKind
    {
        Resources = 0,
        PheromoneFood = 1,
        PheromoneDanger = 2,
        Molecules = 3,
        Mycel = 4
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ms_params_t
    {
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
        public float mycel_inhibitor_weight;
        public float mycel_inhibitor_gain;
        public float mycel_inhibitor_decay;
        public float mycel_inhibitor_threshold;

        public float agent_move_cost;
        public float agent_harvest;
        public float agent_deposit_scale;
        public float agent_sense_radius;
        public float agent_random_turn;
        public float info_metabolism_cost;

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

        public int toxic_enable;
        public float toxic_max_fraction;
        public int toxic_stride_min;
        public int toxic_stride_max;
        public int toxic_iters_min;
        public int toxic_iters_max;

        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
        public float[] toxic_max_fraction_by_quadrant;

        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
        public float[] toxic_max_fraction_by_species;

        public int logic_mode;
        public int logic_input_ax;
        public int logic_input_ay;
        public int logic_input_bx;
        public int logic_input_by;
        public int logic_output_x;
        public int logic_output_y;
        public int logic_pulse_period;
        public float logic_pulse_strength;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct ms_config_t
    {
        public ms_params_t params;
        public uint seed;
    }

    private static class Native
    {
        [DllImport("micro_swarm", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr ms_create(ref ms_config_t cfg);

        [DllImport("micro_swarm", CallingConvention = CallingConvention.Cdecl)]
        public static extern void ms_destroy(IntPtr h);

        [DllImport("micro_swarm", CallingConvention = CallingConvention.Cdecl)]
        public static extern int ms_step(IntPtr h, int steps);

        [DllImport("micro_swarm", CallingConvention = CallingConvention.Cdecl)]
        public static extern void ms_get_field_info(IntPtr h, int kind, out int w, out int hgt);

        [DllImport("micro_swarm", CallingConvention = CallingConvention.Cdecl)]
        public static extern int ms_copy_field_out(IntPtr h, int kind, [Out] float[] dst, int dstCount);

        [DllImport("micro_swarm", CallingConvention = CallingConvention.Cdecl)]
        public static extern int ms_copy_field_in(IntPtr h, int kind, [In] float[] src, int srcCount);
    }

    [Header("Simulation")]
    public int width = 128;
    public int height = 128;
    public int agents = 512;
    public int stepsPerFrame = 1;
    public uint seed = 42;
    public bool autoRun = true;

    [Header("Visualization")]
    public FieldKind fieldKind = FieldKind.PheromoneFood;
    public bool normalize = true;
    public bool useRFloat = true;
    public Renderer targetRenderer;

    [Header("Brush")]
    public bool enableBrush = true;
    public FieldKind brushField = FieldKind.PheromoneFood;
    public float brushValue = 1.0f;
    public int brushRadius = 4;
    public Camera rayCamera;

    private IntPtr _ctx = IntPtr.Zero;
    private Texture2D _texture;
    private float[] _fieldBuffer;
    private Color32[] _colorBuffer;
    private int _fieldW;
    private int _fieldH;
    private bool _disposed;

    private void Start()
    {
        CreateContext();
        CreateTexture();
        UpdateTexture();
    }

    private void Update()
    {
        if (_ctx == IntPtr.Zero)
            return;

        if (autoRun && stepsPerFrame > 0)
        {
            int rc = Native.ms_step(_ctx, stepsPerFrame);
            if (rc <= 0)
            {
                Debug.LogError("ms_step failed");
                return;
            }
        }

        UpdateTexture();
        HandleBrushInput();
    }

    private void OnDestroy()
    {
        Dispose();
    }

    public void Dispose()
    {
        if (_disposed)
            return;

        if (_ctx != IntPtr.Zero)
        {
            Native.ms_destroy(_ctx);
            _ctx = IntPtr.Zero;
        }

        _disposed = true;
    }

    private void CreateContext()
    {
        var cfg = new ms_config_t
        {
            params = new ms_params_t
            {
                width = width,
                height = height,
                agent_count = agents,
                steps = 0,
                info_metabolism_cost = 0.005f,
                mycel_inhibitor_weight = 0.9f,
                mycel_inhibitor_gain = 0.12f,
                mycel_inhibitor_decay = 0.02f,
                mycel_inhibitor_threshold = 0.45f,
                toxic_enable = 1,
                toxic_max_fraction = 1.0f,
                toxic_stride_min = 1,
                toxic_stride_max = 64,
                toxic_iters_min = 0,
                toxic_iters_max = 256,
                toxic_max_fraction_by_quadrant = new float[] {1f, 1f, 1f, 1f},
                toxic_max_fraction_by_species = new float[] {1f, 1f, 1f, 1f},
                logic_mode = 0,
                logic_input_ax = -1,
                logic_input_ay = -1,
                logic_input_bx = -1,
                logic_input_by = -1,
                logic_output_x = -1,
                logic_output_y = -1,
                logic_pulse_period = 20,
                logic_pulse_strength = 10.0f
            },
            seed = seed
        };

        _ctx = Native.ms_create(ref cfg);
        if (_ctx == IntPtr.Zero)
        {
            Debug.LogError("ms_create returned NULL");
        }
    }

    private void CreateTexture()
    {
        if (_ctx == IntPtr.Zero)
            return;

        Native.ms_get_field_info(_ctx, (int)fieldKind, out _fieldW, out _fieldH);
        int count = _fieldW * _fieldH;
        _fieldBuffer = new float[count];

        if (useRFloat)
        {
            _texture = new Texture2D(_fieldW, _fieldH, TextureFormat.RFloat, false, true);
        }
        else
        {
            _texture = new Texture2D(_fieldW, _fieldH, TextureFormat.RGBA32, false, true);
            _colorBuffer = new Color32[count];
        }

        _texture.wrapMode = TextureWrapMode.Clamp;
        _texture.filterMode = FilterMode.Point;

        if (targetRenderer != null)
        {
            targetRenderer.material.mainTexture = _texture;
        }
    }

    private void UpdateTexture()
    {
        if (_ctx == IntPtr.Zero || _texture == null)
            return;

        Native.ms_get_field_info(_ctx, (int)fieldKind, out _fieldW, out _fieldH);
        int count = _fieldW * _fieldH;
        if (_fieldBuffer == null || _fieldBuffer.Length != count)
        {
            _fieldBuffer = new float[count];
            if (!useRFloat)
                _colorBuffer = new Color32[count];
        }

        int rc = Native.ms_copy_field_out(_ctx, (int)fieldKind, _fieldBuffer, _fieldBuffer.Length);
        if (rc <= 0)
        {
            Debug.LogError("ms_copy_field_out failed");
            return;
        }

        float minVal = 0.0f;
        float maxVal = 1.0f;
        if (normalize)
        {
            minVal = float.MaxValue;
            maxVal = float.MinValue;
            for (int i = 0; i < _fieldBuffer.Length; i++)
            {
                float v = _fieldBuffer[i];
                if (v < minVal) minVal = v;
                if (v > maxVal) maxVal = v;
            }
            if (maxVal <= minVal)
            {
                maxVal = minVal + 1.0f;
            }
        }

        if (useRFloat)
        {
            if (normalize)
            {
                for (int i = 0; i < _fieldBuffer.Length; i++)
                {
                    _fieldBuffer[i] = (_fieldBuffer[i] - minVal) / (maxVal - minVal);
                }
            }
            _texture.SetPixelData(_fieldBuffer, 0);
        }
        else
        {
            for (int i = 0; i < _fieldBuffer.Length; i++)
            {
                float v = _fieldBuffer[i];
                if (normalize)
                    v = (v - minVal) / (maxVal - minVal);
                byte c = (byte)Mathf.Clamp(Mathf.RoundToInt(v * 255.0f), 0, 255);
                _colorBuffer[i] = new Color32(c, c, c, 255);
            }
            _texture.SetPixels32(_colorBuffer);
        }

        _texture.Apply(false, false);
    }

    private void HandleBrushInput()
    {
        if (!enableBrush || _ctx == IntPtr.Zero)
            return;

        if (rayCamera == null)
            rayCamera = Camera.main;

        if (rayCamera == null)
            return;

        if (Input.GetMouseButton(0))
        {
            Ray ray = rayCamera.ScreenPointToRay(Input.mousePosition);
            if (Physics.Raycast(ray, out RaycastHit hit))
            {
                Vector2 uv = hit.textureCoord;
                PaintAtUV(uv, brushValue, brushRadius);
            }
        }
    }

    private void PaintAtUV(Vector2 uv, float value, int radius)
    {
        if (_ctx == IntPtr.Zero || _fieldBuffer == null)
            return;

        Native.ms_get_field_info(_ctx, (int)brushField, out _fieldW, out _fieldH);
        int count = _fieldW * _fieldH;
        if (_fieldBuffer.Length != count)
            _fieldBuffer = new float[count];

        int rc = Native.ms_copy_field_out(_ctx, (int)brushField, _fieldBuffer, _fieldBuffer.Length);
        if (rc <= 0)
            return;

        int cx = Mathf.Clamp((int)(uv.x * _fieldW), 0, _fieldW - 1);
        int cy = Mathf.Clamp((int)(uv.y * _fieldH), 0, _fieldH - 1);

        int r2 = radius * radius;
        for (int dy = -radius; dy <= radius; dy++)
        {
            int y = cy + dy;
            if (y < 0 || y >= _fieldH)
                continue;

            for (int dx = -radius; dx <= radius; dx++)
            {
                int x = cx + dx;
                if (x < 0 || x >= _fieldW)
                    continue;

                if (dx * dx + dy * dy > r2)
                    continue;

                int idx = y * _fieldW + x;
                _fieldBuffer[idx] = value;
            }
        }

        rc = Native.ms_copy_field_in(_ctx, (int)brushField, _fieldBuffer, _fieldBuffer.Length);
        if (rc <= 0)
        {
            Debug.LogError("ms_copy_field_in failed");
        }
    }
}
