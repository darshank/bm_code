/*
 * ==========================================================
 * ARM64 Bare-Metal Ray Tracer (SVE2 Accelerated)
 * Features:
 *  - Golden Reference Mode (-DGENERATE_GOLDEN): Generates 10 PPM files in reference_ppm/
 *  - Regression Mode: Validates against reference_images.h
 *  - Objects: Spheres, Planes, Cylinder, Disk, Torus (BVH Triangles)
 *  - Lighting: Phong (Ambient + Diffuse)
 * ==========================================================
 */

#include <stdint.h>
#include <arm_sve.h>

#ifdef GENERATE_GOLDEN
#include <stdio.h>
#include <sys/stat.h>
#endif

#define WIDTH  640
#define HEIGHT 480
#define AMBIENT 0.1f
#define NUM_SCENES 10
#define IMG_SIZE (WIDTH * HEIGHT * 3)

#define NUM_SPHERES    5
#define NUM_PLANES     2
#define NUM_CYLINDERS  1
#define NUM_DISKS      1
#define MAX_TRIANGLES 128

/* UART for bare-metal output */
static volatile uint32_t *UART0 = (uint32_t *) 0x09000000;
static inline void uart_putc(char c)
{
  while (*(UART0 + 6) & (1 << 5));
  *UART0 = c;
}

static inline void uart_puts(const char *s)
{
  while (*s)
    uart_putc(*s++);
}

/* Math utilities */
static inline float sqrtf(float x)
{
  if (x <= 0)
    return 0;
  float g = x * 0.5f;
  for (int i = 0; i < 5; i++)
    g = 0.5f * (g + x / g);
  return g;
}

static inline float fmaxf(float a, float b)
{
  return (a > b) ? a : b;
}

static inline float fminf(float a, float b)
{
  return (a < b) ? a : b;
}

/* Trig approximations for torus mesh */
static inline float my_sinf(float x)
{
  while (x > 3.14159f)
    x -= 6.28318f;
  while (x < -3.14159f)
    x += 6.28318f;
  float x2 = x * x;
  return x * (1 - x2 / 6 + (x2 * x2) / 120);
}

static inline float my_cosf(float x)
{
  while (x > 3.14159f)
    x -= 6.28318f;
  while (x < -3.14159f)
    x += 6.28318f;
  float x2 = x * x;
  return 1 - x2 / 2 + (x2 * x2) / 24;
}

/* Objects */
typedef struct {
  float x, y, z, r;
  uint32_t color;
} Sphere;
typedef struct {
  float nx, ny, nz, d;
  uint32_t color;
} Plane;
typedef struct {
  float cx, cy, cz, r, h;
  uint32_t color;
} Cylinder;
typedef struct {
  float cx, cy, cz, r;
  uint32_t color;
} Disk;
typedef struct {
  float v0[3], v1[3], v2[3];
  uint32_t color;
} Triangle;

static Sphere spheres[NUM_SPHERES];
static Plane planes[NUM_PLANES];
static Cylinder cylinders[NUM_CYLINDERS];
static Disk disks[NUM_DISKS];
static Triangle triangles[MAX_TRIANGLES];
static int num_triangles = 0;
static float light_dir[3] = { 1, 1, -1 };

static uint8_t framebuffer[IMG_SIZE];

/* Normalization */
static inline void normalize_light(float v[3])
{
  float len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  v[0] /= len;
  v[1] /= len;
  v[2] /= len;
}

#ifdef GENERATE_GOLDEN
static void ensure_directory(void)
{
  mkdir("reference_ppm", 0777);
}

static void save_ppm(const char *fn)
{
  FILE *f = fopen(fn, "wb");
  if (!f)
    return;
  fprintf(f, "P6\n%d %d\n255\n", WIDTH, HEIGHT);
  fwrite(framebuffer, 1, IMG_SIZE, f);
  fclose(f);
}
#else
#include "reference_images.h"
#endif

/* Add triangle to BVH list */
static void add_triangle(float v0[3], float v1[3], float v2[3], uint32_t color)
{
  if (num_triangles >= MAX_TRIANGLES)
    return;
  Triangle *t = &triangles[num_triangles++];
  for (int i = 0; i < 3; i++) {
    t->v0[i] = v0[i];
    t->v1[i] = v1[i];
    t->v2[i] = v2[i];
  }
  t->color = color;
}

/* Initialize torus mesh */
static void init_torus(void)
{
  num_triangles = 0;
  float R = 1.5f, r = 0.5f;
  int seg = 8;
  for (int i = 0; i < seg; i++) {
    for (int j = 0; j < seg; j++) {
      float t0 = (float)i / seg * 6.28318f;
      float t1 = (float)(i + 1) / seg * 6.28318f;
      float p0 = (float)j / seg * 6.28318f;
      float p1 = (float)(j + 1) / seg * 6.28318f;
      float v0[3] =
        { (R + r * my_cosf(p0)) * my_cosf(t0), r * my_sinf(p0),
      -(R + r * my_cosf(p0)) * my_sinf(t0) };
      float v1[3] =
        { (R + r * my_cosf(p1)) * my_cosf(t0), r * my_sinf(p1),
      -(R + r * my_cosf(p1)) * my_sinf(t0) };
      float v2[3] =
        { (R + r * my_cosf(p0)) * my_cosf(t1), r * my_sinf(p0),
      -(R + r * my_cosf(p0)) * my_sinf(t1) };
      float v3[3] =
        { (R + r * my_cosf(p1)) * my_cosf(t1), r * my_sinf(p1),
      -(R + r * my_cosf(p1)) * my_sinf(t1) };
      add_triangle(v0, v1, v2, 0xFF8800);
      add_triangle(v2, v1, v3, 0xFF8800);
    }
  }
}

/* Build BVH (flat) */
static void build_bvh(void)
{                               /* No-op: flat traversal */
}

/* Scene variations for 10 golden images */
static void render_scene(int sid)
{
  light_dir[0] = 1;
  light_dir[1] = 1;
  light_dir[2] = -1;
  for (int i = 0; i < NUM_SPHERES; i++) {
    spheres[i].x = (i % 3) * 2 - 2;
    spheres[i].y = (i / 3) - 1;
    spheres[i].z = -3 - i;
    spheres[i].r = 0.8f + (i % 2) * 0.2f;
    spheres[i].color = 0xFF0000 >> (i * 4);
  }
  planes[0] = (Plane) {
  0, 1, 0, -1, 0xAAAAAA};
  planes[1] = (Plane) {
  0, 0, 1, 6, 0xCCCCCC};
  cylinders[0] = (Cylinder) {
  0, -1, -5, 1.0f, 2.0f, 0x00FFFF};
  disks[0] = (Disk) {
  2, -1, -4, 1.5f, 0xFFFF00};
  switch (sid) {
  case 0:
    break;
  case 1:
    light_dir[0] = -1;
    planes[0].color = 0xFF00FF;
    spheres[0].x = -3;
    break;
  case 2:
    for (int i = 0; i < NUM_SPHERES; i++)
      spheres[i].r = 1.2f;
    cylinders[0].color = 0xFFFF00;
    break;
  case 3:
    disks[0].r = 3.0f;
    disks[0].color = 0xFF00FF;
    break;
  case 4:
    light_dir[0] = 0;
    light_dir[1] = -1;
    spheres[1].x += 2;
    spheres[2].x -= 2;
    break;
  case 5:
    init_torus();
    break;
  case 6:
    cylinders[0].h = 4.0f;
    cylinders[0].r = 0.7f;
    break;
  case 7:
    spheres[0].z = -10;
    spheres[4].z = -12;
    planes[1].color = 0x00FF00;
    break;
  case 8:
    planes[0].color = 0x000000;
    for (int i = 0; i < NUM_SPHERES; i++) {
      spheres[i].r = 0.4f;
      spheres[i].color = 0xFFFFFF;
    } break;
  case 9:
    spheres[0].z = -1.5f;
    spheres[1].z = -2.0f;
    init_torus();
    break;
  }
  normalize_light(light_dir);
}

/* Macro for updating hit */
#define UPDATE_HIT(closer,t,nx,ny,nz,r,g,b) \
    *bestT=svsel(closer,t,*bestT);*Nx=svsel(closer,nx,*Nx);*Ny=svsel(closer,ny,*Ny);*Nz=svsel(closer,nz,*Nz);\
    *R=svsel(closer,r,*R);*G=svsel(closer,g,*G);*B=svsel(closer,b,*B);

/* Intersection: Spheres */
static void intersect_spheres(svbool_t pg, svfloat32_t Ox, svfloat32_t Oy,
                              svfloat32_t Oz, svfloat32_t Dx, svfloat32_t Dy,
                              svfloat32_t Dz, svfloat32_t *bestT,
                              svfloat32_t *R, svfloat32_t *G, svfloat32_t *B,
                              svfloat32_t *Nx, svfloat32_t *Ny, svfloat32_t *Nz)
{
  for (int i = 0; i < NUM_SPHERES; i++) {
    float cx = spheres[i].x, cy = spheres[i].y, cz = spheres[i].z, r =
      spheres[i].r;
    uint32_t color = spheres[i].color;
    svfloat32_t X = svsub_f32_z(pg, Ox, svdup_f32(cx));
    svfloat32_t Y = svsub_f32_z(pg, Oy, svdup_f32(cy));
    svfloat32_t Z = svsub_f32_z(pg, Oz, svdup_f32(cz));
    svfloat32_t a =
      svadd_f32_z(pg,
                  svadd_f32_z(pg, svmul_f32_z(pg, Dx, Dx),
                              svmul_f32_z(pg, Dy, Dy)), svmul_f32_z(pg, Dz,
                                                                    Dz));
    svfloat32_t b = svmul_f32_z(pg, Dx, X);
    b = svadd_f32_z(pg, b, svmul_f32_z(pg, Dy, Y));
    b = svadd_f32_z(pg, b, svmul_f32_z(pg, Dz, Z));
    b = svmul_f32_z(pg, b, svdup_f32(2.0f));
    svfloat32_t c =
      svadd_f32_z(pg,
                  svadd_f32_z(pg, svmul_f32_z(pg, X, X), svmul_f32_z(pg, Y, Y)),
                  svmul_f32_z(pg, Z, Z));
    c = svsub_f32_z(pg, c, svdup_f32(r * r));
    svfloat32_t disc =
      svsub_f32_z(pg, svmul_f32_z(pg, b, b),
                  svmul_f32_z(pg, svdup_f32(4), svmul_f32_z(pg, a, c)));
    svbool_t hit = svcmpgt(pg, disc, svdup_f32(0));
    if (!svptest_any(pg, hit))
      continue;
    svfloat32_t sqrt_disc = svsqrt_f32_z(hit, disc);
    svfloat32_t t0 =
      svdiv_f32_z(hit, svsub_f32_z(hit, svneg_f32_z(hit, b), sqrt_disc),
                  svmul_f32_z(hit, a, svdup_f32(2)));
    svbool_t closer =
      svand_z(pg, hit,
              svand_z(pg, svcmplt(pg, t0, *bestT),
                      svcmpgt(pg, t0, svdup_f32(0))));
    if (!svptest_any(pg, closer))
      continue;
    svfloat32_t Hx = svmad_f32_z(closer, Dx, t0, Ox);
    svfloat32_t Hy = svmad_f32_z(closer, Dy, t0, Oy);
    svfloat32_t Hz = svmad_f32_z(closer, Dz, t0, Oz);
    svfloat32_t nx =
      svdiv_f32_z(closer, svsub_f32_z(closer, Hx, svdup_f32(cx)), svdup_f32(r));
    svfloat32_t ny =
      svdiv_f32_z(closer, svsub_f32_z(closer, Hy, svdup_f32(cy)), svdup_f32(r));
    svfloat32_t nz =
      svdiv_f32_z(closer, svsub_f32_z(closer, Hz, svdup_f32(cz)), svdup_f32(r));
    UPDATE_HIT(closer, t0, nx, ny, nz,
               svdup_f32(((color >> 16) & 0xFF) / 255.0f),
               svdup_f32(((color >> 8) & 0xFF) / 255.0f),
               svdup_f32((color & 0xFF) / 255.0f));
  }
}

/* Intersection: Planes */
static void intersect_planes(svbool_t pg, svfloat32_t Ox, svfloat32_t Oy,
                             svfloat32_t Oz, svfloat32_t Dx, svfloat32_t Dy,
                             svfloat32_t Dz, svfloat32_t *bestT, svfloat32_t *R,
                             svfloat32_t *G, svfloat32_t *B, svfloat32_t *Nx,
                             svfloat32_t *Ny, svfloat32_t *Nz)
{
  for (int i = 0; i < NUM_PLANES; i++) {
    float nx = planes[i].nx, ny = planes[i].ny, nz = planes[i].nz, d =
      planes[i].d;
    uint32_t color = planes[i].color;
    svfloat32_t denom =
      svadd_f32_z(pg,
                  svadd_f32_z(pg, svmul_f32_z(pg, Dx, svdup_f32(nx)),
                              svmul_f32_z(pg, Dy, svdup_f32(ny))),
                  svmul_f32_z(pg, Dz, svdup_f32(nz)));
    svbool_t valid = svcmpne(pg, denom, svdup_f32(0));
    svfloat32_t t =
      svdiv_f32_z(valid,
                  svsub_f32_z(valid, svdup_f32(-d),
                              svadd_f32_z(valid,
                                          svadd_f32_z(valid,
                                                      svmul_f32_z(valid, Ox,
                                                                  svdup_f32
                                                                  (nx)),
                                                      svmul_f32_z(valid, Oy,
                                                                  svdup_f32
                                                                  (ny))),
                                          svmul_f32_z(valid, Oz,
                                                      svdup_f32(nz)))), denom);
    svbool_t closer =
      svand_z(pg, valid,
              svand_z(pg, svcmplt(pg, t, *bestT),
                      svcmpgt(pg, t, svdup_f32(0))));
    if (!svptest_any(pg, closer))
      continue;
    UPDATE_HIT(closer, t, svdup_f32(nx), svdup_f32(ny), svdup_f32(nz),
               svdup_f32(((color >> 16) & 0xFF) / 255.0f),
               svdup_f32(((color >> 8) & 0xFF) / 255.0f),
               svdup_f32((color & 0xFF) / 255.0f));
  }
}

/* Intersection: Cylinders */
static void intersect_cylinders(svbool_t pg, svfloat32_t Ox, svfloat32_t Oy,
                                svfloat32_t Oz, svfloat32_t Dx, svfloat32_t Dy,
                                svfloat32_t Dz, svfloat32_t *bestT,
                                svfloat32_t *R, svfloat32_t *G, svfloat32_t *B,
                                svfloat32_t *Nx, svfloat32_t *Ny,
                                svfloat32_t *Nz)
{
  for (int i = 0; i < NUM_CYLINDERS; i++) {
    float cx = cylinders[i].cx, cy = cylinders[i].cy, cz = cylinders[i].cz, r =
      cylinders[i].r, h = cylinders[i].h;
    uint32_t color = cylinders[i].color;
    svfloat32_t X = svsub_f32_z(pg, Ox, svdup_f32(cx));
    svfloat32_t Z = svsub_f32_z(pg, Oz, svdup_f32(cz));
    svfloat32_t a =
      svadd_f32_z(pg, svmul_f32_z(pg, Dx, Dx), svmul_f32_z(pg, Dz, Dz));
    svfloat32_t b = svmul_f32_z(pg, Dx, X);
    b = svadd_f32_z(pg, b, svmul_f32_z(pg, Dz, Z));
    b = svmul_f32_z(pg, b, svdup_f32(2.0f));
    svfloat32_t c =
      svadd_f32_z(pg, svmul_f32_z(pg, X, X), svmul_f32_z(pg, Z, Z));
    c = svsub_f32_z(pg, c, svdup_f32(r * r));
    svfloat32_t disc =
      svsub_f32_z(pg, svmul_f32_z(pg, b, b),
                  svmul_f32_z(pg, svdup_f32(4), svmul_f32_z(pg, a, c)));
    svbool_t hit = svcmpgt(pg, disc, svdup_f32(0));
    if (!svptest_any(pg, hit))
      continue;
    svfloat32_t sqrt_disc = svsqrt_f32_z(hit, disc);
    svfloat32_t t0 =
      svdiv_f32_z(hit, svsub_f32_z(hit, svneg_f32_z(hit, b), sqrt_disc),
                  svmul_f32_z(hit, a, svdup_f32(2)));
    svbool_t closer = svand_z(pg, hit, svcmplt(pg, t0, *bestT));
    if (!svptest_any(pg, closer))
      continue;
    svfloat32_t yhit = svmad_f32_z(closer, Dy, t0, Oy);
    svbool_t within = svand_z(pg, closer, svcmpge(pg, yhit, svdup_f32(cy)));
    within = svand_z(pg, within, svcmple(pg, yhit, svdup_f32(cy + h)));
    if (!svptest_any(pg, within))
      continue;
    svfloat32_t nx =
      svdiv_f32_z(within, svmad_f32_z(within, Dx, t0, Ox), svdup_f32(r));
    svfloat32_t nz =
      svdiv_f32_z(within, svmad_f32_z(within, Dz, t0, Oz), svdup_f32(r));
    UPDATE_HIT(within, t0, nx, svdup_f32(0), nz,
               svdup_f32(((color >> 16) & 0xFF) / 255.0f),
               svdup_f32(((color >> 8) & 0xFF) / 255.0f),
               svdup_f32((color & 0xFF) / 255.0f));
  }
}

/* Intersection: Disks */
static void intersect_disks(svbool_t pg, svfloat32_t Ox, svfloat32_t Oy,
                            svfloat32_t Oz, svfloat32_t Dx, svfloat32_t Dy,
                            svfloat32_t Dz, svfloat32_t *bestT, svfloat32_t *R,
                            svfloat32_t *G, svfloat32_t *B, svfloat32_t *Nx,
                            svfloat32_t *Ny, svfloat32_t *Nz)
{
  for (int i = 0; i < NUM_DISKS; i++) {
    float cx = disks[i].cx, cy = disks[i].cy, cz = disks[i].cz, r = disks[i].r;
    uint32_t color = disks[i].color;
    svfloat32_t denom = Dy;
    svbool_t valid = svcmpne(pg, denom, svdup_f32(0));
    svfloat32_t t =
      svdiv_f32_z(valid, svsub_f32_z(valid, svdup_f32(cy), Oy), Dy);
    svbool_t closer =
      svand_z(pg, valid,
              svand_z(pg, svcmplt(pg, t, *bestT),
                      svcmpgt(pg, t, svdup_f32(0))));
    if (!svptest_any(pg, closer))
      continue;
    svfloat32_t hx = svmad_f32_z(closer, Dx, t, Ox);
    svfloat32_t hz = svmad_f32_z(closer, Dz, t, Oz);
    svfloat32_t dx = svsub_f32_z(closer, hx, svdup_f32(cx));
    svfloat32_t dz = svsub_f32_z(closer, hz, svdup_f32(cz));
    svfloat32_t dist2 =
      svadd_f32_z(closer, svmul_f32_z(closer, dx, dx),
                  svmul_f32_z(closer, dz, dz));
    svbool_t inside = svand_z(pg, closer, svcmple(pg, dist2, svdup_f32(r * r)));
    if (!svptest_any(pg, inside))
      continue;
    UPDATE_HIT(inside, t, svdup_f32(0), svdup_f32(1), svdup_f32(0),
               svdup_f32(((color >> 16) & 0xFF) / 255.0f),
               svdup_f32(((color >> 8) & 0xFF) / 255.0f),
               svdup_f32((color & 0xFF) / 255.0f));
  }
}

/* BVH brute force */
static void bvh_trace_packet_sve(svbool_t pg, svfloat32_t Ox, svfloat32_t Oy,
                                 svfloat32_t Oz, svfloat32_t Dx, svfloat32_t Dy,
                                 svfloat32_t Dz, svfloat32_t *bestT,
                                 svfloat32_t *R, svfloat32_t *G, svfloat32_t *B,
                                 svfloat32_t *Nx, svfloat32_t *Ny,
                                 svfloat32_t *Nz)
{
  for (int i = 0; i < num_triangles; i++) {
    Triangle *t = &triangles[i];
    float v0x = t->v0[0], v0y = t->v0[1], v0z = t->v0[2];
    float v1x = t->v1[0], v1y = t->v1[1], v1z = t->v1[2];
    float v2x = t->v2[0], v2y = t->v2[1], v2z = t->v2[2];
    svfloat32_t e1x = svdup_f32(v1x - v0x), e1y = svdup_f32(v1y - v0y), e1z =
      svdup_f32(v1z - v0z);
    svfloat32_t e2x = svdup_f32(v2x - v0x), e2y = svdup_f32(v2y - v0y), e2z =
      svdup_f32(v2z - v0z);
    svfloat32_t px =
      svsub_f32_z(pg, svmul_f32_z(pg, Dy, e2z), svmul_f32_z(pg, Dz, e2y));
    svfloat32_t py =
      svsub_f32_z(pg, svmul_f32_z(pg, Dz, e2x), svmul_f32_z(pg, Dx, e2z));
    svfloat32_t pz =
      svsub_f32_z(pg, svmul_f32_z(pg, Dx, e2y), svmul_f32_z(pg, Dy, e2x));
    svfloat32_t det =
      svadd_f32_z(pg,
                  svadd_f32_z(pg, svmul_f32_z(pg, e1x, px),
                              svmul_f32_z(pg, e1y, py)), svmul_f32_z(pg, e1z,
                                                                     pz));
    svbool_t hit = svcmpne(pg, det, svdup_f32(0));
    if (!svptest_any(pg, hit))
      continue;
    svfloat32_t inv_det = svdiv_f32_z(hit, svdup_f32(1.0f), det);
    svfloat32_t tx = svsub_f32_z(pg, Ox, svdup_f32(v0x));
    svfloat32_t ty = svsub_f32_z(pg, Oy, svdup_f32(v0y));
    svfloat32_t tz = svsub_f32_z(pg, Oz, svdup_f32(v0z));
    svfloat32_t u =
      svadd_f32_z(pg,
                  svadd_f32_z(pg, svmul_f32_z(pg, tx, px),
                              svmul_f32_z(pg, ty, py)), svmul_f32_z(pg, tz,
                                                                    pz));
    u = svmul_f32_z(hit, u, inv_det);
    svfloat32_t qx =
      svsub_f32_z(pg, svmul_f32_z(pg, ty, e1z), svmul_f32_z(pg, tz, e1y));
    svfloat32_t qy =
      svsub_f32_z(pg, svmul_f32_z(pg, tz, e1x), svmul_f32_z(pg, tx, e1z));
    svfloat32_t qz =
      svsub_f32_z(pg, svmul_f32_z(pg, tx, e1y), svmul_f32_z(pg, ty, e1x));
    svfloat32_t v =
      svadd_f32_z(pg,
                  svadd_f32_z(pg, svmul_f32_z(pg, Dx, qx),
                              svmul_f32_z(pg, Dy, qy)), svmul_f32_z(pg, Dz,
                                                                    qz));
    v = svmul_f32_z(hit, v, inv_det);
    svbool_t inside =
      svand_z(pg, hit,
              svand_z(pg, svcmpge(pg, u, svdup_f32(0)),
                      svand_z(pg, svcmpge(pg, v, svdup_f32(0)),
                              svcmple(pg, svadd_f32_z(pg, u, v),
                                      svdup_f32(1)))));
    if (!svptest_any(pg, inside))
      continue;
    svfloat32_t tval =
      svadd_f32_z(pg,
                  svadd_f32_z(pg, svmul_f32_z(pg, e2x, qx),
                              svmul_f32_z(pg, e2y, qy)), svmul_f32_z(pg, e2z,
                                                                     qz));
    tval = svmul_f32_z(inside, tval, inv_det);
    svbool_t closer = svand_z(pg, inside, svcmplt(pg, tval, *bestT));
    if (!svptest_any(pg, closer))
      continue;
    float rcol = ((t->color >> 16) & 0xFF) / 255.0f, gcol =
      ((t->color >> 8) & 0xFF) / 255.0f, bcol = (t->color & 0xFF) / 255.0f;
    *bestT = svsel(closer, tval, *bestT);
    *R = svsel(closer, svdup_f32(rcol), *R);
    *G = svsel(closer, svdup_f32(gcol), *G);
    *B = svsel(closer, svdup_f32(bcol), *B);
  }
}

/* Apply Phong lighting */
static void apply_phong(svbool_t pg, svfloat32_t Nx, svfloat32_t Ny,
                        svfloat32_t Nz, svfloat32_t *R, svfloat32_t *G,
                        svfloat32_t *B)
{
  float lx = light_dir[0], ly = light_dir[1], lz = light_dir[2];
  float len = sqrtf(lx * lx + ly * ly + lz * lz);
  lx /= len;
  ly /= len;
  lz /= len;
  svfloat32_t Lx = svdup_f32(lx), Ly = svdup_f32(ly), Lz = svdup_f32(lz);
  svfloat32_t ndotl =
    svadd_f32_z(pg,
                svadd_f32_z(pg, svmul_f32_z(pg, Nx, Lx),
                            svmul_f32_z(pg, Ny, Ly)), svmul_f32_z(pg, Nz, Lz));
  ndotl = svmax_f32_z(pg, ndotl, svdup_f32(0));
  svfloat32_t amb = svdup_f32(AMBIENT);
  *R = svmad_f32_z(pg, *R, ndotl, amb);
  *G = svmad_f32_z(pg, *G, ndotl, amb);
  *B = svmad_f32_z(pg, *B, ndotl, amb);
}

/* Render tile (full image) */
static void render_tile(void)
{
  for (int y = 0; y < HEIGHT; y++) {
    float aspect = (float)WIDTH / HEIGHT;
    float v = 1.0f - 2.0f * ((y + 0.5f) / HEIGHT);
    for (int x = 0; x < WIDTH; x += svcntw()) {
      svbool_t pg = svwhilelt_b32(x, WIDTH);
      svint32_t idx_i = svindex_s32(0, 1);
      svfloat32_t idx_f = svcvt_f32_s32_x(pg, idx_i);
      svfloat32_t u = svadd_f32_z(pg, svdup_f32((float)x + 0.5f), idx_f);
      u = svmul_f32_z(pg, u, svdup_f32(2.0f / WIDTH));
      u = svsub_f32_z(pg, u, svdup_f32(1.0f));
      u = svmul_f32_z(pg, u, svdup_f32(aspect));
      svfloat32_t Dx = u, Dy = svdup_f32(v), Dz = svdup_f32(-1.0f);
      svfloat32_t Ox = svdup_f32(0), Oy = svdup_f32(0), Oz = svdup_f32(0);
      svfloat32_t bestT = svdup_f32(1e9f), R = svdup_f32(0), G =
        svdup_f32(0), B = svdup_f32(0), Nx = svdup_f32(0), Ny =
        svdup_f32(0), Nz = svdup_f32(1);
      intersect_spheres(pg, Ox, Oy, Oz, Dx, Dy, Dz, &bestT, &R, &G, &B, &Nx,
                        &Ny, &Nz);
      intersect_planes(pg, Ox, Oy, Oz, Dx, Dy, Dz, &bestT, &R, &G, &B, &Nx, &Ny,
                       &Nz);
      intersect_cylinders(pg, Ox, Oy, Oz, Dx, Dy, Dz, &bestT, &R, &G, &B, &Nx,
                          &Ny, &Nz);
      intersect_disks(pg, Ox, Oy, Oz, Dx, Dy, Dz, &bestT, &R, &G, &B, &Nx, &Ny,
                      &Nz);
      bvh_trace_packet_sve(pg, Ox, Oy, Oz, Dx, Dy, Dz, &bestT, &R, &G, &B, &Nx,
                           &Ny, &Nz);
      apply_phong(pg, Nx, Ny, Nz, &R, &G, &B);
      int lanes = svcntw();
      float tmp[svcntw()];
      svst1(pg, tmp, R);
      for (int i = 0; i < lanes && (x + i) < WIDTH; i++) {
        int idx = (y * WIDTH + (x + i)) * 3;
        framebuffer[idx] = (uint8_t) (fminf(fmaxf(tmp[i], 0), 1) * 255);
      }
      svst1(pg, tmp, G);
      for (int i = 0; i < lanes && (x + i) < WIDTH; i++) {
        int idx = (y * WIDTH + (x + i)) * 3;
        framebuffer[idx + 1] = (uint8_t) (fminf(fmaxf(tmp[i], 0), 1) * 255);
      }
      svst1(pg, tmp, B);
      for (int i = 0; i < lanes && (x + i) < WIDTH; i++) {
        int idx = (y * WIDTH + (x + i)) * 3;
        framebuffer[idx + 2] = (uint8_t) (fminf(fmaxf(tmp[i], 0), 1) * 255);
      }
    }
  }
}

#ifndef GENERATE_GOLDEN
static int validate_framebuffer(int sid)
{
  for (int i = 0; i < IMG_SIZE; i++) {
    if (framebuffer[i] != reference_images[sid][i])
      return 0;
  }
  return 1;
}
#endif

void main(void)
{
#ifdef GENERATE_GOLDEN
  ensure_directory();
#endif
  printf("Rendering Scenes \n");
  for (int s = 0; s < NUM_SCENES; s++) {
    render_scene(s);
    init_torus();
    build_bvh();
    render_tile();
#ifdef GENERATE_GOLDEN
    char fn[128];
    snprintf(fn, sizeof(fn), "reference_ppm/output_%d.ppm", s);
    save_ppm(fn);
#else
    if (!validate_framebuffer(s)) {
      printf("TEST FAIL\n");
      while (1);
    }
    printf("scene %d Pass \n", s);
#endif
  }
#ifndef GENERATE_GOLDEN
  printf("TEST PASS\n");
#endif
//  while (1);
}
