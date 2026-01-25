__kernel void diffuse_and_evaporate(__global const float *input,
                                    __global float *output,
                                    int width,
                                    int height,
                                    float diffusion,
                                    float evaporation) {
    int x = (int)get_global_id(0);
    int y = (int)get_global_id(1);
    if (x >= width || y >= height) {
        return;
    }
    int idx = y * width + x;
    float center = input[idx];

    if (x == 0 || y == 0 || x == width - 1 || y == height - 1) {
        float value = center * (1.0f - evaporation);
        output[idx] = fmax(value, 0.0f);
        return;
    }

    float sum = center * (1.0f - diffusion);
    sum += input[idx - 1] * (diffusion * 0.25f);
    sum += input[idx + 1] * (diffusion * 0.25f);
    sum += input[idx - width] * (diffusion * 0.25f);
    sum += input[idx + width] * (diffusion * 0.25f);

    float value = sum * (1.0f - evaporation);
    output[idx] = fmax(value, 0.0f);
}

__kernel void contention_stamp(__global volatile int *slot,
                               int value,
                               int iterations) {
    for (int i = 0; i < iterations; ++i) {
        atomic_xchg(slot, value);
    }
}
