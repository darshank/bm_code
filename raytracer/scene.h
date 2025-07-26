#ifndef SCENE_H
#define SCENE_H

#include <stdint.h>

#define NUM_SPHERES    5
#define NUM_PLANES     2
#define NUM_CYLINDERS  1
#define NUM_DISKS      1

typedef struct {
    float x, y, z;     /* Center */
    float r;           /* Radius */
    uint32_t color;    /* RGB color (0xRRGGBB) */
} Sphere;

typedef struct {
    float nx, ny, nz;  /* Normal */
    float d;           /* Distance from origin */
    uint32_t color;    /* RGB color */
} Plane;

typedef struct {
    float cx, cy, cz;  /* Base center */
    float r;           /* Radius */
    float h;           /* Height along Y-axis */
    uint32_t color;    /* RGB color */
} Cylinder;

typedef struct {
    float cx, cy, cz;  /* Center */
    float r;           /* Radius */
    uint32_t color;    /* RGB color */
} Disk;

extern Sphere spheres[NUM_SPHERES];
extern Plane planes[NUM_PLANES];
extern Cylinder cylinders[NUM_CYLINDERS];
extern Disk disks[NUM_DISKS];
extern float light_dir[3];

#endif

