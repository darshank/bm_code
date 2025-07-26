#include "bvh.h"
#include <stdint.h>

#define MAX_TRIANGLES 128

typedef struct {
    float v0[3], v1[3], v2[3];
    uint32_t color;
} Triangle;

static Triangle triangles[MAX_TRIANGLES];
static int num_triangles = 0;

/* Minimal trig approximation */
static inline float my_sinf(float x){
    while(x>3.14159265f)x-=6.2831853f;
    while(x<-3.14159265f)x+=6.2831853f;
    float x2=x*x;
    return x*(1 - x2/6 + (x2*x2)/120);
}
static inline float my_cosf(float x){
    while(x>3.14159265f)x-=6.2831853f;
    while(x<-3.14159265f)x+=6.2831853f;
    float x2=x*x;
    return 1 - x2/2 + (x2*x2)/24;
}

static void add_triangle(float v0[3],float v1[3],float v2[3],uint32_t color){
    if(num_triangles>=MAX_TRIANGLES)return;
    Triangle*t=&triangles[num_triangles++];
    for(int i=0;i<3;i++){t->v0[i]=v0[i];t->v1[i]=v1[i];t->v2[i]=v2[i];}
    t->color=color;
}

void init_torus(void){
    num_triangles=0;
    float R=1.5f,r=0.5f;
    int seg=8;
    for(int i=0;i<seg;i++){
        for(int j=0;j<seg;j++){
            float t0=(float)i/seg*6.283185f;
            float t1=(float)(i+1)/seg*6.283185f;
            float p0=(float)j/seg*6.283185f;
            float p1=(float)(j+1)/seg*6.283185f;

            float v0[3]={(R+r*my_cosf(p0))*my_cosf(t0),r*my_sinf(p0),-(R+r*my_cosf(p0))*my_sinf(t0)};
            float v1[3]={(R+r*my_cosf(p1))*my_cosf(t0),r*my_sinf(p1),-(R+r*my_cosf(p1))*my_sinf(t0)};
            float v2[3]={(R+r*my_cosf(p0))*my_cosf(t1),r*my_sinf(p0),-(R+r*my_cosf(p0))*my_sinf(t1)};
            float v3[3]={(R+r*my_cosf(p1))*my_cosf(t1),r*my_sinf(p1),-(R+r*my_cosf(p1))*my_sinf(t1)};

            add_triangle(v0,v1,v2,0xFF8800);
            add_triangle(v2,v1,v3,0xFF8800);
        }
    }
}

void build_bvh(void){/* Flat BVH (just triangles in array) */}

/* BVH brute-force traversal with SVE intersection */
void bvh_trace_packet_sve(
    svbool_t pg, svfloat32_t Ox,svfloat32_t Oy,svfloat32_t Oz,
    svfloat32_t Dx,svfloat32_t Dy,svfloat32_t Dz,
    svfloat32_t*bestT,svfloat32_t*R,svfloat32_t*G,svfloat32_t*B,
    svfloat32_t*Nx,svfloat32_t*Ny,svfloat32_t*Nz){
    for(int i=0;i<num_triangles;i++){
        Triangle*t=&triangles[i];
        float v0x=t->v0[0],v0y=t->v0[1],v0z=t->v0[2];
        float v1x=t->v1[0],v1y=t->v1[1],v1z=t->v1[2];
        float v2x=t->v2[0],v2y=t->v2[1],v2z=t->v2[2];

        svfloat32_t e1x=svdup_f32(v1x-v0x),e1y=svdup_f32(v1y-v0y),e1z=svdup_f32(v1z-v0z);
        svfloat32_t e2x=svdup_f32(v2x-v0x),e2y=svdup_f32(v2y-v0y),e2z=svdup_f32(v2z-v0z);

        svfloat32_t px=svsub_f32_z(pg,svmul_f32_z(pg,Dy,e2z),svmul_f32_z(pg,Dz,e2y));
        svfloat32_t py=svsub_f32_z(pg,svmul_f32_z(pg,Dz,e2x),svmul_f32_z(pg,Dx,e2z));
        svfloat32_t pz=svsub_f32_z(pg,svmul_f32_z(pg,Dx,e2y),svmul_f32_z(pg,Dy,e2x));

        svfloat32_t det=svadd_f32_z(pg,svadd_f32_z(pg,svmul_f32_z(pg,e1x,px),svmul_f32_z(pg,e1y,py)),svmul_f32_z(pg,e1z,pz));
        svbool_t hit=svcmpne(pg,det,svdup_f32(0));
        if(!svptest_any(pg,hit))continue;

        svfloat32_t inv_det=svdiv_f32_z(hit,svdup_f32(1.0f),det);
        svfloat32_t tx=svsub_f32_z(pg,Ox,svdup_f32(v0x));
        svfloat32_t ty=svsub_f32_z(pg,Oy,svdup_f32(v0y));
        svfloat32_t tz=svsub_f32_z(pg,Oz,svdup_f32(v0z));

        svfloat32_t u=svadd_f32_z(pg,svadd_f32_z(pg,svmul_f32_z(pg,tx,px),svmul_f32_z(pg,ty,py)),svmul_f32_z(pg,tz,pz));
        u=svmul_f32_z(hit,u,inv_det);

        svfloat32_t qx=svsub_f32_z(pg,svmul_f32_z(pg,ty,e1z),svmul_f32_z(pg,tz,e1y));
        svfloat32_t qy=svsub_f32_z(pg,svmul_f32_z(pg,tz,e1x),svmul_f32_z(pg,tx,e1z));
        svfloat32_t qz=svsub_f32_z(pg,svmul_f32_z(pg,tx,e1y),svmul_f32_z(pg,ty,e1x));

        svfloat32_t v=svadd_f32_z(pg,svadd_f32_z(pg,svmul_f32_z(pg,Dx,qx),svmul_f32_z(pg,Dy,qy)),svmul_f32_z(pg,Dz,qz));
        v=svmul_f32_z(hit,v,inv_det);

        svbool_t inside=svand_z(pg,hit,svand_z(pg,svcmpge(pg,u,svdup_f32(0)),svand_z(pg,svcmpge(pg,v,svdup_f32(0)),svcmple(pg,svadd_f32_z(pg,u,v),svdup_f32(1)))));
        if(!svptest_any(pg,inside))continue;

        svfloat32_t tval=svadd_f32_z(pg,svadd_f32_z(pg,svmul_f32_z(pg,e2x,qx),svmul_f32_z(pg,e2y,qy)),svmul_f32_z(pg,e2z,qz));
        tval=svmul_f32_z(inside,tval,inv_det);

        svbool_t closer=svand_z(pg,inside,svcmplt(pg,tval,*bestT));
        if(!svptest_any(pg,closer))continue;

        float rcol=((t->color>>16)&0xFF)/255.0f;
        float gcol=((t->color>>8)&0xFF)/255.0f;
        float bcol=(t->color&0xFF)/255.0f;

        *bestT=svsel(closer,tval,*bestT);
        *R=svsel(closer,svdup_f32(rcol),*R);
        *G=svsel(closer,svdup_f32(gcol),*G);
        *B=svsel(closer,svdup_f32(bcol),*B);
    }
}

