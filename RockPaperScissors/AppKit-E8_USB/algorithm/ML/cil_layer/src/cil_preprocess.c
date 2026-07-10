/*
 * Common Inference Library - preprocessing (source implementation)
 *
 * Clean-room replacement for the prebuilt cil.a preprocess path. Produces
 * bit-identical results to the binary for the paths this application uses
 * (RGB888 input at model resolution, float32 output, NHWC or NCHW):
 * the binary evaluates, per element,
 *     out = ((float)px * (1/255.0f) + (-mean_c)) / std_c
 * with a separate multiply, add, and divide (VMUL/VADD/VDIV, no FMA
 * contraction), using the ImageNet constants below.
 *
 * Unlike the binary (convert pass + normalize pass through the arena),
 * this implementation is a single fused pass from the input image to the
 * output tensor: no arena traffic, no intermediate image copies.
 */

#pragma STDC FP_CONTRACT OFF

#include "cil.h"
#include "model_config.h"

/* ImageNet normalization constants, means stored negated as in cil.a */
static const float neg_mean[3] = { -0.485f, -0.456f, -0.406f };
static const float std_dev[3]  = {  0.229f,  0.224f,  0.225f };
#define INV_255 (1.0f / 255.0f)

/* Per-channel lookup tables: normalized value for each possible byte.
   Built once with exactly the binary's op sequence (mul, add, divide), so
   every entry is bit-identical to computing it per pixel - but the hot loop
   becomes a table load instead of a scalar VDIV (MVE has no vector float
   divide, so the divide would otherwise dominate). */
static float norm_lut[3][256];
static int   norm_lut_ready = 0;

static void build_norm_lut(void)
{
    for (int c = 0; c < 3; c++) {
        for (int v = 0; v < 256; v++) {
            float t = (float)v * INV_255;
            t = t + neg_mean[c];
            norm_lut[c][v] = t / std_dev[c];
        }
    }
    norm_lut_ready = 1;
}

status_t preprocess(const uint8_t *image_data,
                    uint32_t image_width,
                    uint32_t image_height,
                    void *preprocessed_data,
                    uint8_t *preprocess_arena,
                    image_color_format_t input_image_color_format,
                    image_input_format_t image_format)
{
    (void)preprocess_arena;   /* fused path needs no scratch memory */

    if ((image_data == NULL) || (preprocessed_data == NULL)) {
        return STATUS_INVALID_ARG;
    }

    if (!norm_lut_ready) {
        build_norm_lut();
    }

    /* RGB and BGR interleaved 8-bit input supported; the application feeds
       RGB888 frames already scaled to the model resolution. */
    uint32_t r_off, b_off;
    switch (input_image_color_format) {
        case IMAGE_COLOR_RGB: r_off = 0U; b_off = 2U; break;
        case IMAGE_COLOR_BGR: r_off = 2U; b_off = 0U; break;
        default:              return STATUS_INVALID_ARG;
    }

    const uint32_t mw = MODEL_INPUT_WIDTH;
    const uint32_t mh = MODEL_INPUT_HEIGHT;
    float *out = (float *)preprocessed_data;

    if ((image_width == mw) && (image_height == mh)) {
        const uint32_t n = mw * mh;
        if (image_format == NCHW) {
            float *out_r = out;
            float *out_g = out + n;
            float *out_b = out + 2U * n;
            const uint8_t *p = image_data;
            for (uint32_t i = 0U; i < n; i++, p += 3U) {
                out_r[i] = norm_lut[0][p[r_off]];
                out_g[i] = norm_lut[1][p[1]];
                out_b[i] = norm_lut[2][p[b_off]];
            }
        } else { /* NHWC */
            const uint8_t *p = image_data;
            float *q = out;
            for (uint32_t i = 0U; i < n; i++, p += 3U, q += 3U) {
                q[0] = norm_lut[0][p[r_off]];
                q[1] = norm_lut[1][p[1]];
                q[2] = norm_lut[2][p[b_off]];
            }
        }
        return STATUS_OK;
    }

    /* Input not at model resolution: fused nearest-neighbour resize +
       normalize (not used by this application; camera frames arrive
       pre-scaled). */
    for (uint32_t y = 0U; y < mh; y++) {
        const uint32_t sy = (y * image_height) / mh;
        for (uint32_t x = 0U; x < mw; x++) {
            const uint32_t sx = (x * image_width) / mw;
            const uint8_t *p = image_data + ((sy * image_width) + sx) * 3U;
            const float r = norm_lut[0][p[r_off]];
            const float g = norm_lut[1][p[1]];
            const float b = norm_lut[2][p[b_off]];
            const uint32_t i = (y * mw) + x;
            if (image_format == NCHW) {
                out[i]                = r;
                out[(mw * mh) + i]    = g;
                out[(2U * mw * mh) + i] = b;
            } else {
                out[i * 3U]      = r;
                out[i * 3U + 1U] = g;
                out[i * 3U + 2U] = b;
            }
        }
    }
    return STATUS_OK;
}

int preprocess_arena_space(int image_width, int image_height,
                           int model_input_width, int model_input_height,
                           image_color_format_t input_image_color_format)
{
    (void)image_width; (void)image_height;
    (void)model_input_width; (void)model_input_height;
    (void)input_image_color_format;
    /* The fused implementation works directly from the input image into the
       output tensor and needs no working memory. */
    return 0;
}
