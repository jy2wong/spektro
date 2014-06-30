#include <stdint.h>
#include <math.h>

/**
 * 0th order modified bessel function of the first kind.
 */
static float I0(float x){
    float v=1;
    float lastv=0;
    float t=1;
    int i;

    x= x*x/4;
    for(i=1; v != lastv; i++){
        lastv=v;
        t *= x/(i*i);
        v += t;
    }
    return v;
}

void create_kb_window(float *windowbuf, unsigned int n, float alpha)
{
    unsigned int k, n2 = (n >> 1);
    float alpha_i0inv = 1.0f / I0(alpha);
    for(k=0; k<n2; k++) {
        float tmp = fabsf((float)k/(float)n);
        tmp = sqrtf(1.0f - tmp*tmp);
        windowbuf[k] = I0(alpha * tmp)*alpha_i0inv;
    }
    for(k=n2; k<n; k++) {
        float tmp = fabsf((float)(n-k)/(float)n);
        tmp = sqrtf(1.0f - tmp*tmp);
        windowbuf[k] = I0(alpha * tmp)*alpha_i0inv;
    }
}

