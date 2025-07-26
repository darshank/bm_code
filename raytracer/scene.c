#include "scene.h"

Sphere spheres[NUM_SPHERES] = {
    {  0.0f,  0.0f, -3.0f, 1.0f, 0xFF0000 },
    {  2.0f,  0.0f, -4.0f, 1.0f, 0x00FF00 },
    { -2.0f,  0.0f, -4.5f, 1.0f, 0x0000FF },
    {  1.0f, -1.0f, -5.0f, 0.8f, 0xFFFF00 },
    { -1.5f, -0.5f, -6.0f, 1.2f, 0xFF00FF }
};

Plane planes[NUM_PLANES] = {
    { 0.0f, 1.0f, 0.0f, -1.0f, 0xAAAAAA },
    { 0.0f, 0.0f, 1.0f,  6.0f, 0xCCCCCC }
};

Cylinder cylinders[NUM_CYLINDERS] = {
    { 0.0f, -1.0f, -5.0f, 1.0f, 2.0f, 0x00FFFF }
};

Disk disks[NUM_DISKS] = {
    { 2.0f, -1.0f, -4.0f, 1.5f, 0xFFFF00 }
};

float light_dir[3] = { 1.0f, 1.0f, -1.0f };

