#include "mycel.h"

#include <algorithm>

MycelNetwork::MycelNetwork(int w, int h)
    : density(w, h, 0.0f),
      inhibitor(w, h, 0.0f),
      width(w),
      height(h) {}

void MycelNetwork::update(const SimParams &params, const GridField &pheromone, const GridField &resources) {
    std::vector<float> next(density.data.size(), 0.0f);
    std::vector<float> next_inhibitor(inhibitor.data.size(), 0.0f);

    auto clamp01 = [](float v) {
        return std::max(0.0f, std::min(1.0f, v));
    };

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float current = density.at(x, y);
            float current_inhib = inhibitor.at(x, y);
            float local_pheromone = pheromone.at(x, y);
            float local_resource = resources.at(x, y);

            float drive = params.mycel_drive_p * local_pheromone + params.mycel_drive_r * local_resource;
            drive = clamp01(drive);
            float threshold = params.mycel_drive_threshold;
            if (drive > threshold) {
                drive = (drive - threshold) / (1.0f - threshold);
            } else {
                drive = 0.0f;
            }

            float neighbor_sum = 0.0f;
            int neighbor_count = 0;
            auto add = [&](int nx, int ny) {
                if (nx < 0 || ny < 0 || nx >= width || ny >= height) {
                    return;
                }
                neighbor_sum += density.at(nx, ny);
                neighbor_count++;
            };

            add(x - 1, y);
            add(x + 1, y);
            add(x, y - 1);
            add(x, y + 1);

            float neighbor_avg = (neighbor_count > 0) ? (neighbor_sum / static_cast<float>(neighbor_count)) : current;
            float transport = params.mycel_transport * (neighbor_avg - current);
            float inhibition = params.mycel_inhibitor_weight * current_inhib;
            if (inhibition < 0.0f) inhibition = 0.0f;
            if (inhibition > 1.0f) inhibition = 1.0f;
            float effective_drive = drive * (1.0f - inhibition);
            float growth = params.mycel_growth * effective_drive * (1.0f - current);
            float decay = params.mycel_decay * current;

            float value = current + growth + transport - decay;
            next[y * width + x] = clamp01(value);

            float inhibitor_drive = current - params.mycel_inhibitor_threshold;
            if (inhibitor_drive < 0.0f) inhibitor_drive = 0.0f;
            float inhib_next = current_inhib + (params.mycel_inhibitor_gain * inhibitor_drive) - (params.mycel_inhibitor_decay * current_inhib);
            next_inhibitor[y * width + x] = clamp01(inhib_next);
        }
    }

    density.data.swap(next);
    inhibitor.data.swap(next_inhibitor);
}
