#pragma once

#include "fields.h"
#include "params.h"

struct MycelNetwork {
    GridField density;
    GridField inhibitor;
    int width = 0;
    int height = 0;

    MycelNetwork() = default;
    MycelNetwork(int w, int h);

    void update(const SimParams &params, const GridField &pheromone, const GridField &resources);
};
