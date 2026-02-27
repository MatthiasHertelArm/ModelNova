/*---------------------------------------------------------------------------
 * Copyright (c) 2025 Arm Limited (or its affiliates). All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *---------------------------------------------------------------------------*/

/**
 * \file image_processing_func_helium.c
 * \brief Helium (MVE) SIMD-optimized image processing overrides.
 *
 * Every public function here is a strong definition that overrides
 * the corresponding __WEAK scalar version in image_processing_func.c.
 * Guarded by __ARM_FEATURE_MVE so the unit compiles to nothing when
 * Helium is not available.
 */

#include <stdint.h>
#include <string.h>
#include <arm_mve.h>
#include "cmsis_compiler.h"
#include "image_processing_func.h"

#if defined(__ARM_FEATURE_MVE) && (__ARM_FEATURE_MVE > 0)

/* ============================================================================
 * Constant scatter-store offset tables (shared by RGB565→RGB888 helpers)
 *
 * For 8 interleaved RGB888 pixels the byte offsets are:
 *   R: 0  3  6  9 12 15 18 21
 *   G: 1  4  7 10 13 16 19 22
 *   B: 2  5  8 11 14 17 20 23
 *
 * Stored as uint16_t because vstrbq_scatter_offset_u16 needs u16 offsets.
 * ============================================================================ */

static const uint16_t rgb888_off_r[8] = { 0,  3,  6,  9, 12, 15, 18, 21};
static const uint16_t rgb888_off_g[8] = { 1,  4,  7, 10, 13, 16, 19, 22};
static const uint16_t rgb888_off_b[8] = { 2,  5,  8, 11, 14, 17, 20, 23};

/* ============================================================================
 * Internal: unpack 8 RGB565 pixels → three u16x8 channels (R8, G8, B8)
 *
 * Uses SIMD shift / mask / or to replicate the top bits into the low bits
 * for full 8-bit expansion, matching the scalar formula:
 *   R8 = (R5 << 3) | (R5 >> 2)
 *   G8 = (G6 << 2) | (G6 >> 4)
 *   B8 = (B5 << 3) | (B5 >> 2)
 * ============================================================================ */

static inline void unpack_rgb565_x8(uint16x8_t px,
                                     uint16x8_t *r8,
                                     uint16x8_t *g8,
                                     uint16x8_t *b8)
{
    uint16x8_t r5 = vshrq_n_u16(px, 11);
    *r8 = vorrq_u16(vshlq_n_u16(r5, 3), vshrq_n_u16(r5, 2));

    uint16x8_t g6 = vandq_u16(vshrq_n_u16(px, 5), vdupq_n_u16(0x3F));
    *g8 = vorrq_u16(vshlq_n_u16(g6, 2), vshrq_n_u16(g6, 4));

    uint16x8_t b5 = vandq_u16(px, vdupq_n_u16(0x1F));
    *b8 = vorrq_u16(vshlq_n_u16(b5, 3), vshrq_n_u16(b5, 2));
}

/* ============================================================================
 * Internal: scatter-store 8 RGB888 pixels using pre-loaded offset vectors
 * ============================================================================ */

static inline void scatter_store_rgb888_x8(uint8_t          *dst,
                                            uint16x8_t        v_r_off,
                                            uint16x8_t        v_g_off,
                                            uint16x8_t        v_b_off,
                                            uint16x8_t        r8,
                                            uint16x8_t        g8,
                                            uint16x8_t        b8)
{
    vstrbq_scatter_offset_u16(dst, v_r_off, r8);
    vstrbq_scatter_offset_u16(dst, v_g_off, g8);
    vstrbq_scatter_offset_u16(dst, v_b_off, b8);
}

/* ============================================================================
 * convert_rgb565_to_rgb888  –  Helium SIMD override
 *
 * Processes 8 pixels per iteration.  Tail handled with MVE predication.
 * ============================================================================ */

void convert_rgb565_to_rgb888(const uint8_t *src,
                              uint8_t       *dst,
                              int            width,
                              int            height)
{
    const uint16_t *src16 = (const uint16_t *)src;
    int total = width * height;

    /* Pre-load scatter offsets once */
    uint16x8_t v_r_off = vldrhq_u16(rgb888_off_r);
    uint16x8_t v_g_off = vldrhq_u16(rgb888_off_g);
    uint16x8_t v_b_off = vldrhq_u16(rgb888_off_b);

    int i = 0;

    /* ---- main SIMD loop: 8 pixels / iteration ---- */
    for (; i <= total - 8; i += 8) {
        uint16x8_t px = vldrhq_u16(&src16[i]);

        uint16x8_t r8, g8, b8;
        unpack_rgb565_x8(px, &r8, &g8, &b8);

        scatter_store_rgb888_x8(&dst[i * 3], v_r_off, v_g_off, v_b_off,
                                r8, g8, b8);
    }

    /* ---- tail: predicated scatter for remaining 1-7 pixels ---- */
    if (i < total) {
        mve_pred16_t p = vctp16q((uint32_t)(total - i));
        uint16x8_t px = vldrhq_z_u16(&src16[i], p);

        uint16x8_t r8, g8, b8;
        unpack_rgb565_x8(px, &r8, &g8, &b8);

        vstrbq_scatter_offset_p_u16(&dst[i * 3], v_r_off, r8, p);
        vstrbq_scatter_offset_p_u16(&dst[i * 3], v_g_off, g8, p);
        vstrbq_scatter_offset_p_u16(&dst[i * 3], v_b_off, b8, p);
    }
}

/* ============================================================================
 * crop_rgb565_to_rgb888  –  Helium SIMD override
 *
 * Identical SIMD core, applied per row with crop offsets.
 * ============================================================================ */

void crop_rgb565_to_rgb888(const uint8_t *src,
                           int src_width,
                           int src_height,
                           uint8_t *dst,
                           int crop_x,
                           int crop_y,
                           int crop_width,
                           int crop_height)
{
    uint16x8_t v_r_off = vldrhq_u16(rgb888_off_r);
    uint16x8_t v_g_off = vldrhq_u16(rgb888_off_g);
    uint16x8_t v_b_off = vldrhq_u16(rgb888_off_b);

    for (int y = 0; y < crop_height; ++y) {
        int src_y = crop_y + y;
        if (src_y >= src_height) break;

        const uint16_t *src_row =
            (const uint16_t *)&src[(src_y * src_width + crop_x) * 2];
        uint8_t *dst_row = &dst[y * crop_width * 3];

        int x = 0;
        for (; x <= crop_width - 8; x += 8) {
            uint16x8_t px = vldrhq_u16(&src_row[x]);

            uint16x8_t r8, g8, b8;
            unpack_rgb565_x8(px, &r8, &g8, &b8);

            scatter_store_rgb888_x8(&dst_row[x * 3],
                                    v_r_off, v_g_off, v_b_off,
                                    r8, g8, b8);
        }

        /* tail */
        if (x < crop_width) {
            mve_pred16_t p = vctp16q((uint32_t)(crop_width - x));
            uint16x8_t px = vldrhq_z_u16(&src_row[x], p);

            uint16x8_t r8, g8, b8;
            unpack_rgb565_x8(px, &r8, &g8, &b8);

            vstrbq_scatter_offset_p_u16(&dst_row[x * 3], v_r_off, r8, p);
            vstrbq_scatter_offset_p_u16(&dst_row[x * 3], v_g_off, g8, p);
            vstrbq_scatter_offset_p_u16(&dst_row[x * 3], v_b_off, b8, p);
        }
    }
}

/* ============================================================================
 * crop_resize_rgb565_to_rgb888  –  NOT overridden
 *
 * Source coordinates differ per destination pixel so gather loads cannot
 * be batched efficiently.  The original scalar implementation in
 * image_processing_func.c is used as-is (it is not __WEAK).
 * ============================================================================ */

/* ============================================================================
 * image_debayer  –  Helium SIMD override
 *
 * Processes 16 raw Bayer pixels per iteration using vpselq to merge the
 * even-column and odd-column colour computations without branching.
 *
 * Neighbour averages use vhaddq_u8 (unsigned halving-add = (a+b)/2)
 * which avoids overflow and is single-cycle on Cortex-M55.
 *
 * The 3-byte interleaved RGB output is written 8 pixels at a time
 * using scatter stores (same approach as the RGB565 functions above).
 * ============================================================================ */

void image_debayer(const uint8_t *raw,
                   uint8_t       *rgb,
                   int            width,
                   int            height,
                   bayer_pattern_t pattern,
                   int            swap_rb)
{
    /*
     * For each Bayer pattern + row-parity combination we need to know,
     * for even-column and odd-column pixels, which of five intermediate
     * values provides R, G, B:
     *
     *   CENTER     – the pixel itself
     *   CROSS_AVG  – avg of 4 cross neighbours (left, right, up, down)
     *   DIAG_AVG   – avg of 4 diagonal neighbours
     *   HORIZ_AVG  – avg of left + right
     *   VERT_AVG   – avg of top + bottom
     *
     * We encode this as an index 0..4 into a small lookup table built
     * per-row so the inner SIMD loop is branch-free.
     */
    enum { CENTER = 0, CROSS_AVG = 1, DIAG_AVG = 2, HORIZ_AVG = 3, VERT_AVG = 4 };

    /* [pattern][row_parity][even/odd col][R/G/B channel] → source index */
    static const uint8_t src_lut[4][2][2][3] = {
        /* RGGB */
        {   /* even row */
            { {CENTER, CROSS_AVG, DIAG_AVG},   /* even col (R pixel) */
              {HORIZ_AVG, CENTER, VERT_AVG} },  /* odd col  (G pixel) */
            /* odd row  */
            { {VERT_AVG, CENTER, HORIZ_AVG},    /* even col (G pixel) */
              {DIAG_AVG, CROSS_AVG, CENTER} }   /* odd col  (B pixel) */
        },
        /* BGGR */
        {   { {DIAG_AVG, CROSS_AVG, CENTER},
              {VERT_AVG, CENTER, HORIZ_AVG} },
            { {CENTER, CROSS_AVG, DIAG_AVG},
              {HORIZ_AVG, CENTER, VERT_AVG} }
        },
        /* GRBG */
        {   { {HORIZ_AVG, CENTER, VERT_AVG},
              {CENTER, CROSS_AVG, DIAG_AVG} },
            { {DIAG_AVG, CROSS_AVG, CENTER},
              {VERT_AVG, CENTER, HORIZ_AVG} }
        },
        /* GBRG */
        {   { {VERT_AVG, CENTER, HORIZ_AVG},
              {DIAG_AVG, CROSS_AVG, CENTER} },
            { {HORIZ_AVG, CENTER, VERT_AVG},
              {CENTER, CROSS_AVG, DIAG_AVG} }
        }
    };

    uint16x8_t v_r_off = vldrhq_u16(rgb888_off_r);
    uint16x8_t v_g_off = vldrhq_u16(rgb888_off_g);
    uint16x8_t v_b_off = vldrhq_u16(rgb888_off_b);

    /*
     * The inner loop starts at x = 1 (odd column).
     * Vector lane 0 → image column 1 (odd).
     * So lanes 0,2,4,… are ODD image columns; lanes 1,3,5,… are EVEN.
     *
     * odd_col_lanes selects the A operand in vpselq: the value for
     * odd-column pixels.
     */
    const mve_pred16_t odd_col_lanes  = 0x5555;

    for (int y = 1; y < height - 1; ++y) {
        int row_parity = y & 1;

        /* Fetch the source assignment for this row */
        const uint8_t *even_col = src_lut[pattern][row_parity][0];
        const uint8_t *odd_col  = src_lut[pattern][row_parity][1];

        const uint8_t *row_cur  = &raw[y * width];
        const uint8_t *row_prev = &raw[(y - 1) * width];
        const uint8_t *row_next = &raw[(y + 1) * width];

        int x = 1;

        /* ---- SIMD path: 16 pixels per iteration ---- */
        for (; x <= width - 2 - 16; x += 16) {
            /* Load 16 centre pixels and shifted neighbours */
            uint8x16_t center   = vldrbq_u8(&row_cur [x]);
            uint8x16_t left     = vldrbq_u8(&row_cur [x - 1]);
            uint8x16_t right    = vldrbq_u8(&row_cur [x + 1]);
            uint8x16_t top      = vldrbq_u8(&row_prev[x]);
            uint8x16_t bottom   = vldrbq_u8(&row_next[x]);
            uint8x16_t top_l    = vldrbq_u8(&row_prev[x - 1]);
            uint8x16_t top_r    = vldrbq_u8(&row_prev[x + 1]);
            uint8x16_t bot_l    = vldrbq_u8(&row_next[x - 1]);
            uint8x16_t bot_r    = vldrbq_u8(&row_next[x + 1]);

            /* Compute the five intermediate values */
            uint8x16_t vals[5];
            vals[CENTER]    = center;
            vals[CROSS_AVG] = vhaddq_u8(vhaddq_u8(left, right),
                                         vhaddq_u8(top, bottom));
            vals[DIAG_AVG]  = vhaddq_u8(vhaddq_u8(top_l, top_r),
                                         vhaddq_u8(bot_l, bot_r));
            vals[HORIZ_AVG] = vhaddq_u8(left, right);
            vals[VERT_AVG]  = vhaddq_u8(top, bottom);

            /* Select R, G, B per-lane using vpselq (odd → A, even → B) */
            uint8x16_t r_ch = vpselq_u8(vals[odd_col[0]],
                                          vals[even_col[0]], odd_col_lanes);
            uint8x16_t g_ch = vpselq_u8(vals[odd_col[1]],
                                          vals[even_col[1]], odd_col_lanes);
            uint8x16_t b_ch = vpselq_u8(vals[odd_col[2]],
                                          vals[even_col[2]], odd_col_lanes);

            /* Handle swap_rb by swapping channel pointers */
            uint8x16_t ch0 = swap_rb ? b_ch : r_ch;
            uint8x16_t ch2 = swap_rb ? r_ch : b_ch;

            /* ---- Write 16 pixels as 2 × 8 with scatter stores ---- */
            uint8_t *out = &rgb[(y * width + x) * 3];

            /* First 8 pixels: widen bottom half to u16 */
            uint16x8_t r_lo = vmovlbq_u8(ch0);
            uint16x8_t g_lo = vmovlbq_u8(g_ch);
            uint16x8_t b_lo = vmovlbq_u8(ch2);
            scatter_store_rgb888_x8(out, v_r_off, v_g_off, v_b_off,
                                    r_lo, g_lo, b_lo);

            /* Second 8 pixels: widen top half */
            uint16x8_t r_hi = vmovltq_u8(ch0);
            uint16x8_t g_hi = vmovltq_u8(g_ch);
            uint16x8_t b_hi = vmovltq_u8(ch2);
            scatter_store_rgb888_x8(out + 24, v_r_off, v_g_off, v_b_off,
                                    r_hi, g_hi, b_hi);
        }

        /* ---- scalar tail for remaining pixels on this row ---- */
        for (; x < width - 1; ++x) {
            int idx = y * width + x;
            int is_even_col = (x & 1) == 0;
            const uint8_t *col = is_even_col ? even_col : odd_col;

            int vals_s[5];
            vals_s[CENTER]    = raw[idx];
            vals_s[CROSS_AVG] = (raw[idx-1] + raw[idx+1] +
                                  raw[idx-width] + raw[idx+width]) >> 2;
            vals_s[DIAG_AVG]  = (raw[idx-width-1] + raw[idx-width+1] +
                                  raw[idx+width-1] + raw[idx+width+1]) >> 2;
            vals_s[HORIZ_AVG] = (raw[idx-1] + raw[idx+1]) >> 1;
            vals_s[VERT_AVG]  = (raw[idx-width] + raw[idx+width]) >> 1;

            int out_idx = idx * 3;
            int r = vals_s[col[0]];
            int g = vals_s[col[1]];
            int b = vals_s[col[2]];

            if (swap_rb) { int t = r; r = b; b = t; }
            rgb[out_idx + 0] = (uint8_t)r;
            rgb[out_idx + 1] = (uint8_t)g;
            rgb[out_idx + 2] = (uint8_t)b;
        }
    }
}

/* ============================================================================
 * image_copy_to_framebuffer  –  Helium SIMD override
 *
 * Fast path: no flip, no swap → 16-byte block copies with tail predication.
 * Fast path: no flip, swap_rb only → gather/scatter R↔B swap, 8 px/iter.
 * Fallback : scalar for flip cases (uncommon, dominated by pixel logic).
 * ============================================================================ */

void image_copy_to_framebuffer(const uint8_t *src,
                               int src_width,
                               int src_height,
                               uint8_t *dst,
                               int dst_width,
                               int dst_height,
                               int x_offset,
                               int y_offset,
                               image_format_t format,
                               int flip_horizontal,
                               int flip_vertical,
                               int swap_rb)
{
    int bpp;
    switch (format) {
        case IMAGE_FORMAT_GRAYSCALE: bpp = 1; break;
        case IMAGE_FORMAT_RGB565:    bpp = 2; break;
        case IMAGE_FORMAT_RGB888:    bpp = 3; break;
        default: return;
    }

    /* ---------- Fast path 1: plain copy (no transforms) ---------- */
    if (!flip_horizontal && !flip_vertical && !swap_rb) {
        for (int y = 0; y < src_height; ++y) {
            int dst_y = y + y_offset;
            if (dst_y < 0 || dst_y >= dst_height) continue;

            int row_bytes = src_width * bpp;
            const uint8_t *s = src + y * row_bytes;
            uint8_t       *d = dst + (dst_y * dst_width + x_offset) * bpp;

            int b = 0;
            /* 16-byte SIMD copies */
            for (; b <= row_bytes - 16; b += 16) {
                uint8x16_t v = vldrbq_u8(&s[b]);
                vstrbq_u8(&d[b], v);
            }
            /* tail predication */
            if (b < row_bytes) {
                mve_pred16_t p = vctp8q((uint32_t)(row_bytes - b));
                uint8x16_t v = vldrbq_z_u8(&s[b], p);
                vstrbq_p_u8(&d[b], v, p);
            }
        }
        return;
    }

    /* ---------- Fast path 2: swap_rb only (RGB888, no flip) ---------- */
    if (!flip_horizontal && !flip_vertical && swap_rb &&
        format == IMAGE_FORMAT_RGB888) {

        uint16x8_t v_r_off = vldrhq_u16(rgb888_off_r);
        uint16x8_t v_g_off = vldrhq_u16(rgb888_off_g);
        uint16x8_t v_b_off = vldrhq_u16(rgb888_off_b);

        for (int y = 0; y < src_height; ++y) {
            int dst_y = y + y_offset;
            if (dst_y < 0 || dst_y >= dst_height) continue;

            const uint8_t *s = src + y * src_width * 3;
            uint8_t       *d = dst + (dst_y * dst_width + x_offset) * 3;

            int x = 0;
            for (; x <= src_width - 8; x += 8) {
                const uint8_t *sp = &s[x * 3];
                uint8_t       *dp = &d[x * 3];

                /* Gather R and B, store swapped */
                uint16x8_t r_vals = vldrbq_gather_offset_u16(sp, v_r_off);
                uint16x8_t g_vals = vldrbq_gather_offset_u16(sp, v_g_off);
                uint16x8_t b_vals = vldrbq_gather_offset_u16(sp, v_b_off);

                vstrbq_scatter_offset_u16(dp, v_r_off, b_vals); /* R ← B */
                vstrbq_scatter_offset_u16(dp, v_g_off, g_vals); /* G ← G */
                vstrbq_scatter_offset_u16(dp, v_b_off, r_vals); /* B ← R */
            }
            /* scalar tail */
            for (; x < src_width; ++x) {
                int si = x * 3;
                int di = x * 3;
                d[di + 0] = s[si + 2];
                d[di + 1] = s[si + 1];
                d[di + 2] = s[si + 0];
            }
        }
        return;
    }

    /* ---------- Fallback: scalar for flip cases ---------- */
    for (int y = 0; y < src_height; ++y) {
        int dst_y = y + y_offset;
        if (dst_y < 0 || dst_y >= dst_height) continue;

        for (int x = 0; x < src_width; ++x) {
            int dst_x = x + x_offset;
            if (dst_x < 0 || dst_x >= dst_width) continue;

            int sy = flip_vertical   ? (src_height - 1 - y) : y;
            int sx = flip_horizontal ? (src_width  - 1 - x) : x;
            int src_idx = (sy * src_width + sx) * bpp;
            int dst_idx = (dst_y * dst_width + dst_x) * bpp;

            if (format == IMAGE_FORMAT_RGB888 && swap_rb) {
                dst[dst_idx + 0] = src[src_idx + 2];
                dst[dst_idx + 1] = src[src_idx + 1];
                dst[dst_idx + 2] = src[src_idx + 0];
            } else {
                for (int i = 0; i < bpp; ++i)
                    dst[dst_idx + i] = src[src_idx + i];
            }
        }
    }
}

/* ============================================================================
 * crop_rgb888_to_rgb888  –  Helium SIMD override
 *
 * Each row is a contiguous memcpy of crop_width × 3 bytes, ideal for
 * 16-byte vector loads / stores with tail predication.
 * ============================================================================ */

void crop_rgb888_to_rgb888(const uint8_t *src,
                           int src_width,
                           int src_height,
                           uint8_t *dst,
                           int crop_x,
                           int crop_y,
                           int crop_width,
                           int crop_height)
{
    const int bpp = 3;

    for (int y = 0; y < crop_height; ++y) {
        int src_y = crop_y + y;
        if (src_y >= src_height) break;

        const uint8_t *s = src + (src_y * src_width + crop_x) * bpp;
        uint8_t       *d = dst + y * crop_width * bpp;

        int row_bytes = crop_width * bpp;
        int b = 0;

        for (; b <= row_bytes - 16; b += 16) {
            uint8x16_t v = vldrbq_u8(&s[b]);
            vstrbq_u8(&d[b], v);
        }
        if (b < row_bytes) {
            mve_pred16_t p = vctp8q((uint32_t)(row_bytes - b));
            uint8x16_t v = vldrbq_z_u8(&s[b], p);
            vstrbq_p_u8(&d[b], v, p);
        }
    }
}

/* ============================================================================
 * image_resize  –  scalar (kept as strong override for linker consistency)
 *
 * Bilinear interpolation with non-uniform source coordinates per
 * destination pixel prevents effective SIMD batching.  The scalar
 * path is retained so all seven __WEAK overrides come from this TU.
 * ============================================================================ */

#define FP_SHIFT 16
#define FP_ONE   (1 << FP_SHIFT)
#define FP_MASK  (FP_ONE - 1)

static inline void unpack_pixel_s(const uint8_t *buf, image_format_t fmt,
                                   int *r, int *g, int *b)
{
    switch (fmt) {
        case IMAGE_FORMAT_GRAYSCALE:
            *r = *g = *b = buf[0];
            break;
        case IMAGE_FORMAT_RGB565: {
            uint16_t px = buf[0] | (buf[1] << 8);
            *r = ((px >> 11) & 0x1F) << 3;
            *g = ((px >>  5) & 0x3F) << 2;
            *b = ( px        & 0x1F) << 3;
            break;
        }
        default: /* RGB888 */
            *r = buf[0]; *g = buf[1]; *b = buf[2];
            break;
    }
}

static inline void pack_pixel_s(uint8_t *buf, image_format_t fmt,
                                 int r, int g, int b)
{
    switch (fmt) {
        case IMAGE_FORMAT_GRAYSCALE:
            buf[0] = (uint8_t)((r * 299 + g * 587 + b * 114) / 1000);
            break;
        case IMAGE_FORMAT_RGB565: {
            uint16_t px = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
            buf[0] = px & 0xFF;
            buf[1] = (px >> 8) & 0xFF;
            break;
        }
        default: /* RGB888 */
            buf[0] = (uint8_t)r;
            buf[1] = (uint8_t)g;
            buf[2] = (uint8_t)b;
            break;
    }
}

void image_resize(const uint8_t *src,
                  int src_width,  int src_height,
                  uint8_t *dst,
                  int dst_width,  int dst_height,
                  image_format_t src_format,
                  image_format_t dst_format)
{
    int src_bpp = (src_format == IMAGE_FORMAT_GRAYSCALE) ? 1 :
                  (src_format == IMAGE_FORMAT_RGB565)    ? 2 : 3;
    int dst_bpp = (dst_format == IMAGE_FORMAT_GRAYSCALE) ? 1 :
                  (dst_format == IMAGE_FORMAT_RGB565)    ? 2 : 3;

    int x_ratio = ((src_width  - 1) << FP_SHIFT) / (dst_width  - 1);
    int y_ratio = ((src_height - 1) << FP_SHIFT) / (dst_height - 1);

    for (int y = 0; y < dst_height; ++y) {
        int src_y_fp = y * y_ratio;
        int y0 = src_y_fp >> FP_SHIFT;
        int y1 = (y0 < src_height - 1) ? y0 + 1 : y0;
        int wy = src_y_fp & FP_MASK;

        for (int x = 0; x < dst_width; ++x) {
            int src_x_fp = x * x_ratio;
            int x0 = src_x_fp >> FP_SHIFT;
            int x1 = (x0 < src_width - 1) ? x0 + 1 : x0;
            int wx = src_x_fp & FP_MASK;

            int r00, g00, b00, r01, g01, b01;
            int r10, g10, b10, r11, g11, b11;

            unpack_pixel_s(&src[(y0*src_width+x0)*src_bpp], src_format, &r00,&g00,&b00);
            unpack_pixel_s(&src[(y0*src_width+x1)*src_bpp], src_format, &r01,&g01,&b01);
            unpack_pixel_s(&src[(y1*src_width+x0)*src_bpp], src_format, &r10,&g10,&b10);
            unpack_pixel_s(&src[(y1*src_width+x1)*src_bpp], src_format, &r11,&g11,&b11);

            int r_t = ((FP_ONE-wx)*r00 + wx*r01) >> FP_SHIFT;
            int r_b = ((FP_ONE-wx)*r10 + wx*r11) >> FP_SHIFT;
            int r   = ((FP_ONE-wy)*r_t + wy*r_b) >> FP_SHIFT;

            int g_t = ((FP_ONE-wx)*g00 + wx*g01) >> FP_SHIFT;
            int g_b = ((FP_ONE-wx)*g10 + wx*g11) >> FP_SHIFT;
            int g_v = ((FP_ONE-wy)*g_t + wy*g_b) >> FP_SHIFT;

            int b_t = ((FP_ONE-wx)*b00 + wx*b01) >> FP_SHIFT;
            int b_b = ((FP_ONE-wx)*b10 + wx*b11) >> FP_SHIFT;
            int b_v = ((FP_ONE-wy)*b_t + wy*b_b) >> FP_SHIFT;

            pack_pixel_s(&dst[(y*dst_width+x)*dst_bpp], dst_format, r, g_v, b_v);
        }
    }
}

#endif /* __ARM_FEATURE_MVE */
