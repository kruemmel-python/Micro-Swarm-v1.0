#pragma once

#include <string>
#include <vector>

#include "sim/fields.h"

class OpenCLRuntime {
public:
    OpenCLRuntime();
    ~OpenCLRuntime();

    bool init(int platform_index, int device_index, std::string &error);
    bool build_kernels(std::string &error);
    void set_kernel_source(std::string source);
    bool assemble_evolved_kernel(const int codons[4], int toxic_stride, int toxic_iters, std::string &error);
    bool assemble_evolved_kernel_quadrant(int quadrant, const int codons[4], int toxic_stride, int toxic_iters, std::string &error);
    void set_quadrant_lws(const int lws[4][2]);
    bool init_fields(const GridField &phero_food,
                     const GridField &phero_danger,
                     const GridField &phero_gamma,
                     const GridField &molecules,
                     std::string &error);
    bool upload_fields(const GridField &phero_food,
                       const GridField &phero_danger,
                       const GridField &phero_gamma,
                       const GridField &molecules,
                       std::string &error);
    bool step_diffuse(const FieldParams &pheromone_params,
                      const FieldParams &molecule_params,
                      bool do_copyback,
                      GridField &phero_food,
                      GridField &phero_danger,
                      GridField &phero_gamma,
                      GridField &molecules,
                      std::string &error);
    bool copyback(GridField &phero_food, GridField &phero_danger, GridField &phero_gamma, GridField &molecules, std::string &error);
    bool is_available() const;
    float last_hardware_exhaustion_ns() const;
    void last_quadrant_exhaustion_ns(float out[4]) const;
    std::string device_info() const;

    static bool print_devices(std::string &output, std::string &error);

private:
    struct Impl;
    Impl *impl;
};
