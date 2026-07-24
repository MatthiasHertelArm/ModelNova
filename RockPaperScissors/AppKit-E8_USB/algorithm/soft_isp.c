/*
 * SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its
 * affiliates <open-source-office@arm.com>
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>

#include "soft_isp.h"

/* Smoothed control state (one stream) */
static int gain_r_q8   = 256;   /* white balance red gain,  256 = 1.0 */
static int gain_b_q8   = 256;   /* white balance blue gain, 256 = 1.0 */
static int gain_exp_q8 = 256;   /* digital exposure gain,   256 = 1.0 */

/* sRGB OETF (gamma encode) table for linear 8-bit input, built once */
static uint8_t gamma_lut[256];
static int     gamma_lut_ready = 0;

static void build_gamma_lut(void)
{
    for (int i = 0; i < 256; ++i) {
        float lum = i * (1.0f / 255.0f);
        float enc;
        if (lum <= 0.0031308f) {
            enc = 12.92f * lum;
        } else {
            /* s = lum^(1/2.4) via Newton iteration on s^12 = lum^5
               (integer powers only, avoids pulling in powf) */
            float lum2 = lum * lum;
            float lum5 = lum2 * lum2 * lum;
            float s    = 1.0f;
            for (int it = 0; it < 12; ++it) {
                float s2  = s * s;
                float s4  = s2 * s2;
                float s11 = s4 * s4 * s2 * s;
                s -= (s11 * s - lum5) / (12.0f * s11);
                if (s < 1e-4f) {
                    s = 1e-4f;
                    break;
                }
            }
            enc = 1.055f * s - 0.055f;
        }
        int v = (int)(enc * 255.0f + 0.5f);
        gamma_lut[i] = (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
    }
    gamma_lut_ready = 1;
}

/* Mild post-gamma sharpening: 5-point unsharp mask, processed with a
   two-row ring of original values so the filter reads unmodified
   neighbors. Border rows/columns are left untouched. */
static void sharpen(uint8_t *rgb, int width, int height, int strength_q8)
{
    /* Row ring: original content of the previous and current rows */
    static uint8_t rowbuf[2][3 * 1024];
    if (width > 1024) {
        return;
    }

    int stride = width * 3;

    memcpy(rowbuf[0], rgb, (size_t)stride);            /* row 0 original */
    for (int y = 1; y < height - 1; ++y) {
        uint8_t       *cur  = rgb + y * stride;
        const uint8_t *below = cur + stride;           /* still original  */
        uint8_t       *ring  = rowbuf[y & 1];
        const uint8_t *above = rowbuf[(y - 1) & 1];    /* original row y-1 */

        memcpy(ring, cur, (size_t)stride);             /* row y original  */

        for (int x = 3; x < stride - 3; ++x) {
            int p    = ring[x];
            int lap  = 4 * p - ring[x - 3] - ring[x + 3] - above[x] - below[x];
            int v    = p + ((lap * strength_q8) >> 10); /* /4 folded in    */
            cur[x]   = (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
        }
    }
}

void SoftISP_Process(uint8_t *rgb, int width, int height,
                     const SoftISP_Config *cfg, SoftISP_Status *status)
{
    if ((rgb == NULL) || (cfg == NULL) || (width <= 0) || (height <= 0)) {
        return;
    }

    if (!gamma_lut_ready) {
        build_gamma_lut();
    }

    int black_level = cfg->black_level;
    if (black_level < 0)  black_level = 0;
    if (black_level > 64) black_level = 64;

    /* Black level correction table: subtract the sensor pedestal and
       rescale to full range (linear domain, before white balance) */
    uint8_t blc_lut[256];
    {
        int span = 255 - black_level;
        for (int i = 0; i < 256; ++i) {
            int v = i - black_level;
            if (v < 0) v = 0;
            blc_lut[i] = (uint8_t)((v * 255 + span / 2) / span);
        }
    }

    /* Radial lens shading index scale: idx = (rpix2 * scale) >> 16, where
       rpix2 is the squared distance from the image center and the last
       table entry corresponds to the image corner */
    int      cx        = width / 2;
    int      cy        = height / 2;
    uint32_t lsc_scale = 0U;
    if ((cfg->lsc_r_q8 != NULL) && (cfg->lsc_r_len > 0)) {
        uint32_t rmax2 = (uint32_t)(cx * cx + cy * cy);
        lsc_scale = (((uint32_t)cfg->lsc_r_len << 16) + rmax2 - 1U) / rmax2;
    }

    /* Center metering box: the middle half of the frame in each dimension */
    int box_x0 = width / 4, box_x1 = (3 * width) / 4;
    int box_y0 = height / 4, box_y1 = (3 * height) / 4;

    /* ---- Statistics pass -------------------------------------------------
       All measurements are on black-level- and shading-corrected linear
       values. Near-black pixels are excluded from the white balance ratios
       (noise dominates there) but counted in the exposure histogram. */
    uint32_t sum_r = 0U, sum_g = 0U, sum_b = 0U;   /* AWB sums (valid px)   */
    uint32_t cnt = 0U, gray_cnt = 0U;              /* AWB valid/gray counts */
    uint32_t hist_g[64] = {0U};                    /* full-frame histogram  */
    uint64_t sum_c = 0U;                           /* center-box g sum      */
    uint32_t cnt_c = 0U;

    int num_px = width * height;
    const uint8_t *p = rgb;
    for (int y = 0; y < height; ++y) {
        int dy2      = (y - cy) * (y - cy);
        int y_in_box = (y >= box_y0) && (y < box_y1);
        for (int x = 0; x < width; ++x, p += 3) {
            uint32_t g = blc_lut[p[1]];
            hist_g[g >> 2]++;

            if (y_in_box && (x >= box_x0) && (x < box_x1)) {
                sum_c += g;
                cnt_c++;
            }

            if (g >= 8U) {
                uint32_t r = blc_lut[p[0]];
                if (lsc_scale != 0U) {
                    uint32_t rpix2 = (uint32_t)(dy2 + (x - cx) * (x - cx));
                    uint32_t idx   = (rpix2 * lsc_scale) >> 16;
                    if (idx >= (uint32_t)cfg->lsc_r_len) {
                        idx = (uint32_t)cfg->lsc_r_len - 1U;
                    }
                    r = (r * cfg->lsc_r_q8[idx]) >> 8;
                    if (r > 255U) {
                        r = 255U;
                    }
                }
                uint32_t b = blc_lut[p[2]];
                sum_r += r;
                sum_g += g;
                sum_b += b;
                cnt++;

                /* AWB confidence: is this pixel near-gray under the CURRENT
                   gains? (residual color error small relative to green) */
                uint32_t rw = (r * (uint32_t)gain_r_q8) >> 8;
                uint32_t bw = (b * (uint32_t)gain_b_q8) >> 8;
                uint32_t tol = g >> 2;
                uint32_t dr = (rw > g) ? (rw - g) : (g - rw);
                uint32_t db = (bw > g) ? (bw - g) : (g - bw);
                if ((g >= 16U) && (dr <= tol) && (db <= tol)) {
                    gray_cnt++;
                }
            }
        }
    }

    /* ---- Exposure: center-weighted mean toward a mid target -------------- */
    uint32_t mean_used;
    {
        uint32_t sum_all = 0U;
        for (int bin = 0; bin < 64; ++bin) {
            sum_all += hist_g[bin] * (((uint32_t)bin << 2) + 2U);
        }
        uint32_t mean_all = sum_all / (uint32_t)num_px;
        uint32_t mean_ctr = (cnt_c > 0U) ? (uint32_t)(sum_c / cnt_c) : mean_all;

        /* mean = (256 * full + w * center) / (256 + w) */
        uint32_t w = (uint32_t)(cfg->ae_center_weight_q8 < 0 ? 0 : cfg->ae_center_weight_q8);
        mean_used  = (256U * mean_all + w * mean_ctr) / (256U + w);

        uint32_t tail = (uint32_t)num_px / 100U; /* ~1% of all pixels */
        uint32_t acc = 0U;
        int p99_bin = 63;
        while (p99_bin > 0) {
            acc += hist_g[p99_bin];
            if (acc >= tail) {
                break;
            }
            p99_bin--;
        }
        uint32_t p99 = ((uint32_t)p99_bin << 2) + 3U;

        if (mean_used > 0U) {
            int target = (cfg->ae_target << 8) / (int)mean_used;
            int limit  = (290 << 8) / (int)(p99 + 1U);  /* allow mild p99 clip */
            if (target > limit) target = limit;
            if (target < 256)  target = 256;            /* never darken        */
            if (target > 2048) target = 2048;           /* at most 8x          */
            gain_exp_q8 += (target - gain_exp_q8) / 4;
        }
    }

    /* ---- White balance: gray-world with confidence freeze ---------------- */
    int gray_pct = (cnt > 0U) ? (int)((gray_cnt * 100U) / cnt) : 0;
    int frozen   = 0;
    if ((cnt > (uint32_t)(num_px / 16)) && (sum_r > 0U) && (sum_b > 0U)) {
        if (gray_pct >= cfg->awb_min_gray_pct) {
            int target_r = (int)(((uint64_t)sum_g << 8) / sum_r);
            int target_b = (int)(((uint64_t)sum_g << 8) / sum_b);
            if (target_r < 128)  target_r = 128;   /* gains in [0.5 .. 8.0] */
            if (target_r > 2048) target_r = 2048;
            if (target_b < 128)  target_b = 128;
            if (target_b > 2048) target_b = 2048;
            /* Converge over a few frames without flicker */
            gain_r_q8 += (target_r - gain_r_q8) / 4;
            gain_b_q8 += (target_b - gain_b_q8) / 4;
        } else {
            /* Too few near-gray pixels: the gray-world assumption is not
               trustworthy for this scene; keep the current gains. */
            frozen = 1;
        }
    }

    /* ---- Saturation-restoring color matrix (Q8, rows sum to 256) --------- */
    int ccm[3][3];
    {
        int s = cfg->saturation_q8;
        if (s < 256) s = 256;
        if (s > 768) s = 768;
        const int luma[3] = {77, 150, 29}; /* 0.299, 0.587, 0.114 in Q8 */
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                ccm[row][col] = (((256 - s) * luma[col]) >> 8) + ((row == col) ? s : 0);
            }
        }
    }

    /* ---- Compose black level + white balance + exposure into linear LUTs.
       Not clamped here: clipping is done per pixel on all three channels
       together so blown highlights stay white. ---------------------------- */
    uint16_t lin_r[256], lin_g[256], lin_b[256];
    for (int i = 0; i < 256; ++i) {
        int v = (blc_lut[i] * gain_exp_q8) >> 8;
        lin_r[i] = (uint16_t)((v * gain_r_q8) >> 8);
        lin_g[i] = (uint16_t)v;
        lin_b[i] = (uint16_t)((v * gain_b_q8) >> 8);
    }

    /* ---- Apply: LUTs (+ radial red gain) -> hue-preserving clip ->
       color matrix -> gamma ------------------------------------------------ */
    uint8_t *q = rgb;
    for (int y = 0; y < height; ++y) {
        int dy2 = (y - cy) * (y - cy);
        for (int x = 0; x < width; ++x, q += 3) {
            int r0 = lin_r[q[0]];
            int g0 = lin_g[q[1]];
            int b0 = lin_b[q[2]];

            if (lsc_scale != 0U) {
                uint32_t rpix2 = (uint32_t)(dy2 + (x - cx) * (x - cx));
                uint32_t idx   = (rpix2 * lsc_scale) >> 16;
                if (idx >= (uint32_t)cfg->lsc_r_len) {
                    idx = (uint32_t)cfg->lsc_r_len - 1U;
                }
                r0 = (r0 * cfg->lsc_r_q8[idx]) >> 8;
            }

            int m = r0 > g0 ? r0 : g0;
            if (b0 > m) m = b0;
            if (m > 255) {
                r0 = (r0 * 255) / m;
                g0 = (g0 * 255) / m;
                b0 = (b0 * 255) / m;
            }

            int r1 = (ccm[0][0] * r0 + ccm[0][1] * g0 + ccm[0][2] * b0) >> 8;
            int g1 = (ccm[1][0] * r0 + ccm[1][1] * g0 + ccm[1][2] * b0) >> 8;
            int b1 = (ccm[2][0] * r0 + ccm[2][1] * g0 + ccm[2][2] * b0) >> 8;

            q[0] = gamma_lut[r1 < 0 ? 0 : (r1 > 255 ? 255 : r1)];
            q[1] = gamma_lut[g1 < 0 ? 0 : (g1 > 255 ? 255 : g1)];
            q[2] = gamma_lut[b1 < 0 ? 0 : (b1 > 255 ? 255 : b1)];
        }
    }

    if (cfg->sharpen_q8 > 0) {
        sharpen(rgb, width, height, cfg->sharpen_q8);
    }

    if (status != NULL) {
        status->mean_linear = mean_used;
        status->gain_r_q8   = gain_r_q8;
        status->gain_b_q8   = gain_b_q8;
        status->gain_exp_q8 = gain_exp_q8;
        status->awb_gray_pct = gray_pct;
        status->awb_frozen   = frozen;
    }
}
