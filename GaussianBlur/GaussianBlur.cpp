//////////////////////////////////////////
// This file contains a Gaussian blur filter
// that applies a Gaussian blur to the input videonode.
// The blur strength is determined by the radius parameter.

#include <stdlib.h>
#include <math.h>
#ifdef __ARM_NEON__
#include <arm_neon.h> // Include NEON header for ARM SIMD
#endif
#include "VapourSynth4.h"
#include "VSHelper4.h"

typedef struct {
    VSNode *node;
    int radius;
} GaussianBlurData;

// This is the main processing function that generates the output frame.
static const VSFrame *VS_CC gaussianBlurGetFrame(int n, int activationReason, void *instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    GaussianBlurData *d = (GaussianBlurData *)instanceData;

    if (activationReason == arInitial) {
        // Request the input frame
        vsapi->requestFrameFilter(n, d->node, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        // Get the input frame
        const VSFrame *src = vsapi->getFrameFilter(n, d->node, frameCtx);
        const VSVideoFormat *fi = vsapi->getVideoFrameFormat(src);
        int height = vsapi->getFrameHeight(src, 0);
        int width = vsapi->getFrameWidth(src, 0);

        // Create the output frame, copying metadata
        VSFrame *dst = vsapi->newVideoFrame(fi, width, height, src, core);

        // Process each plane
        for (int plane = 0; plane < fi->numPlanes; plane++) {
            const uint16_t *srcp = (const uint16_t *)vsapi->getReadPtr(src, plane);
            int src_stride = vsapi->getStride(src, plane) / sizeof(uint16_t);  // Adjust stride for 16-bit format
            uint16_t *dstp = (uint16_t *)vsapi->getWritePtr(dst, plane);
            int dst_stride = vsapi->getStride(dst, plane) / sizeof(uint16_t);
            int plane_width = vsapi->getFrameWidth(src, plane);
            int plane_height = vsapi->getFrameHeight(src, plane);

            // Generate the one-dimensional Gaussian kernel
            int radius = d->radius;
            int kernel_size = 2 * radius + 1;
            double *kernel = (double *)malloc(kernel_size * sizeof(double));
            double sigma = radius / 3.0;
            double sum = 0.0;

            for (int i = -radius; i <= radius; i++) {
                kernel[i + radius] = exp(-(i * i) / (2 * sigma * sigma));
                sum += kernel[i + radius];
            }

            // Normalize the kernel
            for (int i = 0; i < kernel_size; i++) {
                kernel[i] /= sum;
            }

            // Temporary buffer for storing the intermediate result of horizontal blur
            uint16_t *tmp = (uint16_t *)malloc(plane_height * plane_width * sizeof(uint16_t));

#ifdef __ARM_NEON__
            for (int y = 0; y < plane_height; y++) {
                int x = 0;
                for (; x <= plane_width - 8; x += 8) { // Process 8 pixels at a time
                    float32x4_t val_low = vdupq_n_f32(0.0f);
                    float32x4_t val_high = vdupq_n_f32(0.0f);

                    for (int k = -radius; k <= radius; k++) {
                        int xx = x + k;
                        if (xx < 0)
                            xx = 0;
                        else if (xx >= plane_width)
                            xx = plane_width - 1;

                        uint16x8_t src_vals = vld1q_u16(&srcp[y * src_stride + xx]);
                        float32x4_t src_vals_low = vcvtq_f32_u32(vmovl_u16(vget_low_u16(src_vals)));
                        float32x4_t src_vals_high = vcvtq_f32_u32(vmovl_u16(vget_high_u16(src_vals)));

                        float kernel_val = (float)kernel[k + radius];

                        // Multiply and accumulate with the kernel value
                        val_low = vmlaq_n_f32(val_low, src_vals_low, kernel_val);
                        val_high = vmlaq_n_f32(val_high, src_vals_high, kernel_val);
                    }

                    // Convert back to 16-bit and store results
                    uint16x8_t result = vcombine_u16(vqmovn_u32(vcvtq_u32_f32(val_low)),
                                                     vqmovn_u32(vcvtq_u32_f32(val_high)));
                    vst1q_u16(&tmp[y * plane_width + x], result);
                }

                // Handle remaining pixels with scalar code
                for (; x < plane_width; x++) {
                    double val = 0.0;
                    for (int k = -radius; k <= radius; k++) {
                        int xx = x + k;
                        if (xx < 0)
                            xx = 0;
                        else if (xx >= plane_width)
                            xx = plane_width - 1;
                        val += srcp[y * src_stride + xx] * kernel[k + radius];
                    }
                    val = val < 0 ? 0 : val > 65535 ? 65535 : val;
                    tmp[y * plane_width + x] = (uint16_t)(val + 0.5);
                }
            }
#else
            for (int y = 0; y < plane_height; y++) {
                for (int x = 0; x < plane_width; x++) {
                    double val = 0.0;
                    for (int k = -radius; k <= radius; k++) {
                        int xx = x + k;
                        if (xx < 0)
                            xx = 0;
                        else if (xx >= plane_width)
                            xx = plane_width - 1;
                        val += srcp[y * src_stride + xx] * kernel[k + radius];
                    }
                    val = val < 0 ? 0 : val > 65535 ? 65535 : val;
                    tmp[y * plane_width + x] = (uint16_t)(val + 0.5);
                }
            }
#endif

#ifdef __ARM_NEON__
            for (int y = 0; y < plane_height; y++) {
                int x = 0;
                for (; x <= plane_width - 8; x += 8) {
                    float32x4_t val_low = vdupq_n_f32(0.0f);
                    float32x4_t val_high = vdupq_n_f32(0.0f);

                    for (int k = -radius; k <= radius; k++) {
                        int yy = y + k;
                        if (yy < 0)
                            yy = 0;
                        else if (yy >= plane_height)
                            yy = plane_height - 1;

                        uint16x8_t tmp_vals = vld1q_u16(&tmp[yy * plane_width + x]);
                        float32x4_t tmp_vals_low = vcvtq_f32_u32(vmovl_u16(vget_low_u16(tmp_vals)));
                        float32x4_t tmp_vals_high = vcvtq_f32_u32(vmovl_u16(vget_high_u16(tmp_vals)));

                        float kernel_val = (float)kernel[k + radius];

                        // Multiply and accumulate with the kernel value
                        val_low = vmlaq_n_f32(val_low, tmp_vals_low, kernel_val);
                        val_high = vmlaq_n_f32(val_high, tmp_vals_high, kernel_val);
                    }

                    // Convert back to 16-bit and store results
                    uint16x8_t result = vcombine_u16(vqmovn_u32(vcvtq_u32_f32(val_low)),
                                                     vqmovn_u32(vcvtq_u32_f32(val_high)));
                    vst1q_u16(&dstp[y * dst_stride + x], result);
                }

                // Handle remaining pixels with scalar code
                for (; x < plane_width; x++) {
                    double val = 0.0;
                    for (int k = -radius; k <= radius; k++) {
                        int yy = y + k;
                        if (yy < 0)
                            yy = 0;
                        else if (yy >= plane_height)
                            yy = plane_height - 1;
                        val += tmp[yy * plane_width + x] * kernel[k + radius];
                    }
                    val = val < 0 ? 0 : val > 65535 ? 65535 : val;
                    dstp[y * dst_stride + x] = (uint16_t)(val + 0.5);
                }
            }
#else
            for (int y = 0; y < plane_height; y++) {
                for (int x = 0; x < plane_width; x++) {
                    double val = 0.0;
                    for (int k = -radius; k <= radius; k++) {
                        int yy = y + k;
                        if (yy < 0)
                            yy = 0;
                        else if (yy >= plane_height)
                            yy = plane_height - 1;
                        val += tmp[yy * plane_width + x] * kernel[k + radius];
                    }
                    val = val < 0 ? 0 : val > 65535 ? 65535 : val;
                    dstp[y * dst_stride + x] = (uint16_t)(val + 0.5);
                }
            }
#endif

            free(kernel);
            free(tmp);
        }

        // Release the source frame
        vsapi->freeFrame(src);

        return dst;
    }

    return NULL;
}

// Free all allocated data when the filter is destroyed
static void VS_CC gaussianBlurFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    GaussianBlurData *d = (GaussianBlurData *)instanceData;
    vsapi->freeNode(d->node);
    free(d);
}

// This function is responsible for validating arguments and creating the filter instance
static void VS_CC gaussianBlurCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    GaussianBlurData d;
    GaussianBlurData *data;
    int err;

    // Get the input clip
    d.node = vsapi->mapGetNode(in, "clip", 0, 0);
    const VSVideoInfo *vi = vsapi->getVideoInfo(d.node);

    // Only support constant format 16-bit integer input
    if (!vsh::isConstantVideoFormat(vi) || vi->format.sampleType != stInteger || vi->format.bitsPerSample != 16) {
        vsapi->mapSetError(out, "GaussianBlur: Only constant format 16-bit integer input supported.");
        vsapi->freeNode(d.node);
        return;
    }

    // Get the blur radius parameter
    d.radius = (int)vsapi->mapGetInt(in, "radius", 0, &err);
    if (err) {
        vsapi->mapSetError(out, "GaussianBlur: 'radius' parameter is required.");
        vsapi->freeNode(d.node);
        return;
    }

    if (d.radius < 1) {
        vsapi->mapSetError(out, "GaussianBlur: 'radius' must be greater than 0.");
        vsapi->freeNode(d.node);
        return;
    }

    // Allocate filter data
    data = (GaussianBlurData *)malloc(sizeof(d));
    *data = d;

    // Create the filter
    VSFilterDependency deps[] = {{d.node, rpStrictSpatial}};
    vsapi->createVideoFilter(out, "GaussianBlur", vi, gaussianBlurGetFrame, gaussianBlurFree, fmParallel, deps, 1, data, core);
}

//////////////////////////////////////////
// Plugin initialization

VS_EXTERNAL_API(void) VapourSynthPluginInit2(VSPlugin *plugin, const VSPLUGINAPI *vspapi) {
    vspapi->configPlugin("com.yuygfgg.gaussianblur", "gaussblur", "VapourSynth Gaussian Blur Plugin", VS_MAKE_VERSION(1, 0), VAPOURSYNTH_API_VERSION, 0, plugin);
    vspapi->registerFunction("GaussianBlur", "clip:vnode;radius:int;", "clip:vnode;", gaussianBlurCreate, NULL, plugin);
}