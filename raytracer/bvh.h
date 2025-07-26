#ifndef BVH_H
#define BVH_H

#include <arm_sve.h>

/* Initialize torus mesh (triangles for BVH) */
void init_torus(void);

/* Build BVH (currently flat layout) */
void build_bvh(void);

/* BVH traversal for SVE packet */
void bvh_trace_packet_sve(
    svbool_t pg,
    svfloat32_t Ox, svfloat32_t Oy, svfloat32_t Oz,
    svfloat32_t Dx, svfloat32_t Dy, svfloat32_t Dz,
    svfloat32_t *bestT,
    svfloat32_t *R, svfloat32_t *G, svfloat32_t *B,
    svfloat32_t *Nx, svfloat32_t *Ny, svfloat32_t *Nz
);

#endif

