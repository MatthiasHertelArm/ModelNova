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
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *---------------------------------------------------------------------------*/

#include <stdint.h>
#include <stddef.h>
#include "cmsis_compiler.h"
#include "image_processing_func.h"

/*
  Clamp a value to a specified range.

  This function ensures that the input value does not exceed the specified minimum
  and maximum bounds. If the value is less than the minimum, it returns the minimum.
  If it is greater than the maximum, it returns the maximum. Otherwise, it returns
  the value itself.

  \param[in] val  The value to clamp.
  \param[in] min  The minimum bound.
  \param[in] max  The maximum bound.
  \return         The clamped value.
*/
static inline int clamp(int val, int min, int max) {
  return (val < min) ? min : (val > max) ? max : val;
}

__WEAK void image_debayer(const uint8_t *raw,
                          uint8_t *rgb,
                          int width,
                          int height,
                          bayer_pattern_t pattern,
                          int swap_rb) {
  for (int y = 1; y < height - 1; ++y) {
    for (int x = 1; x < width - 1; ++x) {
      int idx = y * width + x;
      int r = 0, g = 0, b = 0;

      int is_even_row = (y & 1) == 0;
      int is_even_col = (x & 1) == 0;

      switch (pattern) {
        case BAYER_PATTERN_RGGB:
          if (is_even_row) {
            if (is_even_col) {
              // Red
              r = raw[idx];
              g = (raw[idx - 1] + raw[idx + 1] + raw[idx - width] + raw[idx + width]) >> 2;
              b = (raw[idx - width - 1] + raw[idx - width + 1] + raw[idx + width - 1] + raw[idx + width + 1]) >> 2;
            } else {
              // Green (on red row)
              g = raw[idx];
              r = (raw[idx - 1] + raw[idx + 1]) >> 1;
              b = (raw[idx - width] + raw[idx + width]) >> 1;
            }
          } else {
            if (is_even_col) {
              // Green (on blue row)
              g = raw[idx];
              r = (raw[idx - width] + raw[idx + width]) >> 1;
              b = (raw[idx - 1] + raw[idx + 1]) >> 1;
            } else {
              // Blue
              b = raw[idx];
              g = (raw[idx - 1] + raw[idx + 1] + raw[idx - width] + raw[idx + width]) >> 2;
              r = (raw[idx - width - 1] + raw[idx - width + 1] + raw[idx + width - 1] + raw[idx + width + 1]) >> 2;
            }
          }
          break;

        case BAYER_PATTERN_BGGR:
          if (is_even_row) {
            if (is_even_col) {
              // Blue
              b = raw[idx];
              g = (raw[idx - 1] + raw[idx + 1] + raw[idx - width] + raw[idx + width]) >> 2;
              r = (raw[idx - width - 1] + raw[idx - width + 1] + raw[idx + width - 1] + raw[idx + width + 1]) >> 2;
            } else {
              // Green (on blue row)
              g = raw[idx];
              b = (raw[idx - 1] + raw[idx + 1]) >> 1;
              r = (raw[idx - width] + raw[idx + width]) >> 1;
            }
          } else {
            if (is_even_col) {
              // Green (on red row)
              g = raw[idx];
              b = (raw[idx - width] + raw[idx + width]) >> 1;
              r = (raw[idx - 1] + raw[idx + 1]) >> 1;
            } else {
              // Red
              r = raw[idx];
              g = (raw[idx - 1] + raw[idx + 1] + raw[idx - width] + raw[idx + width]) >> 2;
              b = (raw[idx - width - 1] + raw[idx - width + 1] + raw[idx + width - 1] + raw[idx + width + 1]) >> 2;
            }
          }
          break;

        case BAYER_PATTERN_GRBG:
          if (is_even_row) {
            if (is_even_col) {
              // Green (on red row)
              g = raw[idx];
              r = (raw[idx - 1] + raw[idx + 1]) >> 1;
              b = (raw[idx - width] + raw[idx + width]) >> 1;
            } else {
              // Red
              r = raw[idx];
              g = (raw[idx - 1] + raw[idx + 1] + raw[idx - width] + raw[idx + width]) >> 2;
              b = (raw[idx - width - 1] + raw[idx - width + 1] + raw[idx + width - 1] + raw[idx + width + 1]) >> 2;
            }
          } else {
            if (is_even_col) {
              // Blue
              b = raw[idx];
              g = (raw[idx - 1] + raw[idx + 1] + raw[idx - width] + raw[idx + width]) >> 2;
              r = (raw[idx - width - 1] + raw[idx - width + 1] + raw[idx + width - 1] + raw[idx + width + 1]) >> 2;
            } else {
              // Green (on blue row)
              g = raw[idx];
              b = (raw[idx - 1] + raw[idx + 1]) >> 1;
              r = (raw[idx - width] + raw[idx + width]) >> 1;
            }
          }
          break;

        case BAYER_PATTERN_GBRG:
          if (is_even_row) {
            if (is_even_col) {
              // Green (on blue row)
              g = raw[idx];
              b = (raw[idx - 1] + raw[idx + 1]) >> 1;
              r = (raw[idx - width] + raw[idx + width]) >> 1;
            } else {
              // Blue
              b = raw[idx];
              g = (raw[idx - 1] + raw[idx + 1] + raw[idx - width] + raw[idx + width]) >> 2;
              r = (raw[idx - width - 1] + raw[idx - width + 1] + raw[idx + width - 1] + raw[idx + width + 1]) >> 2;
            }
          } else {
            if (is_even_col) {
              // Red
              r = raw[idx];
              g = (raw[idx - 1] + raw[idx + 1] + raw[idx - width] + raw[idx + width]) >> 2;
              b = (raw[idx - width - 1] + raw[idx - width + 1] + raw[idx + width - 1] + raw[idx + width + 1]) >> 2;
            } else {
              // Green (on red row)
              g = raw[idx];
              r = (raw[idx - 1] + raw[idx + 1]) >> 1;
              b = (raw[idx - width] + raw[idx + width]) >> 1;
            }
          }
          break;
      }

      int out_idx = (y * width + x) * 3;
      if (swap_rb == 0) {
        rgb[out_idx + 0] = (uint8_t)r;
        rgb[out_idx + 1] = (uint8_t)g;
        rgb[out_idx + 2] = (uint8_t)b;
      } else {
        rgb[out_idx + 0] = (uint8_t)b;
        rgb[out_idx + 1] = (uint8_t)g;
        rgb[out_idx + 2] = (uint8_t)r;
      }
    }
  }
}


void image_gray_world_wb_gamma(uint8_t *img, int width, int height,
                               int black_level, int saturation_q8,
                               const uint16_t *lsc_r_q8, int lsc_r_len) {
  /* sRGB OETF (gamma encode) table for linear 8-bit input, built once */
  static uint8_t gamma_lut[256];
  static int gamma_lut_ready = 0;
  /* Smoothed white balance gains in Q8 (256 = 1.0) */
  static int gain_r_q8 = 256;
  static int gain_b_q8 = 256;
  /* Smoothed digital exposure gain in Q8 (256 = 1.0) */
  static int gain_exp_q8 = 256;

  if (!gamma_lut_ready) {
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
        float s = 1.0f;
        for (int it = 0; it < 12; ++it) {
          float s2 = s * s;
          float s4 = s2 * s2;
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

  if (black_level < 0) black_level = 0;
  if (black_level > 64) black_level = 64;

  /* Black level correction table: subtract the sensor pedestal and rescale
     to full range (in linear space, before white balance) */
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
     rpix2 is the squared distance from the image center and the last table
     entry corresponds to the image corner */
  int cx = width / 2;
  int cy = height / 2;
  uint32_t lsc_scale = 0U;
  if ((lsc_r_q8 != NULL) && (lsc_r_len > 0)) {
    uint32_t rmax2 = (uint32_t)(cx * cx + cy * cy);
    lsc_scale = (((uint32_t)lsc_r_len << 16) + rmax2 - 1U) / rmax2;
  }

  /* Gray-world measurement on black-level- and shading-corrected values;
     skip nearly black pixels where sensor noise dominates the ratios.
     Also histogram the green channel for the exposure gain. */
  uint32_t sum_r = 0U, sum_g = 0U, sum_b = 0U;
  uint32_t cnt = 0U;
  uint32_t hist_g[64] = {0U};
  int num_px = width * height;
  const uint8_t *p = img;
  for (int y = 0; y < height; ++y) {
    int dy2 = (y - cy) * (y - cy);
    for (int x = 0; x < width; ++x, p += 3) {
      int g = blc_lut[p[1]];
      hist_g[g >> 2]++;
      if (g >= 8) {
        uint32_t r = blc_lut[p[0]];
        if (lsc_scale != 0U) {
          uint32_t rpix2 = (uint32_t)(dy2 + (x - cx) * (x - cx));
          uint32_t idx = (rpix2 * lsc_scale) >> 16;
          if (idx >= (uint32_t)lsc_r_len) {
            idx = (uint32_t)lsc_r_len - 1U;
          }
          r = (r * lsc_r_q8[idx]) >> 8;
          if (r > 255U) {
            r = 255U;
          }
        }
        sum_r += r;
        sum_g += (uint32_t)g;
        sum_b += blc_lut[p[2]];
        cnt++;
      }
    }
  }

  /* Digital exposure: lift the linear mean to a mid target, but cap the
     gain so the 99th percentile stays below clipping. Compensates the
     sensor AEC converging on a dark average. */
  if (cnt > (uint32_t)(num_px / 16)) {
    uint32_t mean_g = sum_g / cnt;
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
    if (mean_g > 0U) {
      int target = (110 << 8) / (int)mean_g;        /* reach mid exposure    */
      int limit = (290 << 8) / (int)(p99 + 1U);     /* allow mild p99 clip   */
      if (target > limit) target = limit;
      if (target < 256) target = 256;               /* never darken         */
      if (target > 2048) target = 2048;             /* at most 8x           */
      gain_exp_q8 += (target - gain_exp_q8) / 4;
    }
  }

  if ((sum_r > 0U) && (sum_b > 0U)) {
    int target_r = (int)(((uint64_t)sum_g << 8) / sum_r);
    int target_b = (int)(((uint64_t)sum_g << 8) / sum_b);
    if (target_r < 128) target_r = 128;      /* clamp gains to [0.5 .. 8.0] */
    if (target_r > 2048) target_r = 2048;
    if (target_b < 128) target_b = 128;
    if (target_b > 2048) target_b = 2048;
    /* Converge in a few frames without flicker */
    gain_r_q8 += (target_r - gain_r_q8) / 4;
    gain_b_q8 += (target_b - gain_b_q8) / 4;
  }

  /* Saturation-restoring color matrix in Q8, gray preserving (rows sum to
     256): M = s*I + (1-s)*L with L projecting onto BT.601 luma. Compensates
     the sensor channel crosstalk a calibrated ISP CCM would remove. */
  int ccm[3][3];
  {
    int s = saturation_q8;
    if (s < 256) s = 256;
    if (s > 768) s = 768;
    const int luma[3] = {77, 150, 29}; /* 0.299, 0.587, 0.114 in Q8 */
    for (int row = 0; row < 3; ++row) {
      for (int col = 0; col < 3; ++col) {
        ccm[row][col] = (((256 - s) * luma[col]) >> 8) + ((row == col) ? s : 0);
      }
    }
  }

  /* Compose black level + white balance + exposure into per-channel
     linear LUTs */
  uint8_t lin_r[256], lin_g[256], lin_b[256];
  for (int i = 0; i < 256; ++i) {
    int v = (blc_lut[i] * gain_exp_q8) >> 8;
    int r = (v * gain_r_q8) >> 8;
    int b = (v * gain_b_q8) >> 8;
    lin_r[i] = (uint8_t)(r > 255 ? 255 : r);
    lin_g[i] = (uint8_t)(v > 255 ? 255 : v);
    lin_b[i] = (uint8_t)(b > 255 ? 255 : b);
  }

  /* Apply: linear LUTs (+ radial red gain) -> color matrix -> gamma */
  uint8_t *q = img;
  for (int y = 0; y < height; ++y) {
    int dy2 = (y - cy) * (y - cy);
    for (int x = 0; x < width; ++x, q += 3) {
      int r0 = lin_r[q[0]];
      int g0 = lin_g[q[1]];
      int b0 = lin_b[q[2]];

      if (lsc_scale != 0U) {
        uint32_t rpix2 = (uint32_t)(dy2 + (x - cx) * (x - cx));
        uint32_t idx = (rpix2 * lsc_scale) >> 16;
        if (idx >= (uint32_t)lsc_r_len) {
          idx = (uint32_t)lsc_r_len - 1U;
        }
        r0 = (r0 * lsc_r_q8[idx]) >> 8;
        if (r0 > 255) {
          r0 = 255;
        }
      }

      int r1 = (ccm[0][0] * r0 + ccm[0][1] * g0 + ccm[0][2] * b0) >> 8;
      int g1 = (ccm[1][0] * r0 + ccm[1][1] * g0 + ccm[1][2] * b0) >> 8;
      int b1 = (ccm[2][0] * r0 + ccm[2][1] * g0 + ccm[2][2] * b0) >> 8;

      q[0] = gamma_lut[r1 < 0 ? 0 : (r1 > 255 ? 255 : r1)];
      q[1] = gamma_lut[g1 < 0 ? 0 : (g1 > 255 ? 255 : g1)];
      q[2] = gamma_lut[b1 < 0 ? 0 : (b1 > 255 ? 255 : b1)];
    }
  }
}

__WEAK void crop_and_debayer(const uint8_t *src,
                             int src_width,
                             int src_height,
                             int src_crop_x,
                             int src_crop_y,
                             uint8_t *dst_rgb,
                             int dst_width,
                             int dst_height,
                             bayer_pattern_t pattern) {
  int offsets[2][2];
  switch (pattern) {
    case BAYER_PATTERN_BGGR: offsets[0][0] = 0; offsets[0][1] = 1; offsets[1][0] = 1; offsets[1][1] = 2; break;
    case BAYER_PATTERN_GBRG: offsets[0][0] = 1; offsets[0][1] = 0; offsets[1][0] = 2; offsets[1][1] = 1; break;
    case BAYER_PATTERN_GRBG: offsets[0][0] = 1; offsets[0][1] = 2; offsets[1][0] = 0; offsets[1][1] = 1; break;
    case BAYER_PATTERN_RGGB: offsets[0][0] = 2; offsets[0][1] = 1; offsets[1][0] = 1; offsets[1][1] = 0; break;
  }

  for (int dy = 0; dy < dst_height; ++dy) {
    int sy_fp = (dy * (src_height - 2 - src_crop_y * 2) << 8) / (dst_height - 1); // fixed-point
    int sy = sy_fp >> 8;
    int dy_frac = sy_fp & 0xFF;

    sy += src_crop_y;
    if (sy < 1) {
      sy = 1;
    }
    if (sy >= src_height - 2) {
      sy = src_height - 2;
    }

    for (int dx = 0; dx < dst_width; ++dx) {
      int sx_fp = (dx * (src_width - 2 - src_crop_x * 2) << 8) / (dst_width - 1); // fixed-point
      int sx = sx_fp >> 8;
      int dx_frac = sx_fp & 0xFF;

      sx += src_crop_x;
      if (sx < 1) {
        sx = 1;
      }
      if (sx >= src_width - 2) {
        sx = src_width - 2;
      }

      int row_parity = sy & 1;
      int col_parity = sx & 1;
      int offset = offsets[row_parity][col_parity];

      const uint8_t *p = src;
      int center = p[sy * src_width + sx];

      int r = 0;
      int g = 0;
      int b = 0;

      switch (offset) {
        case 0: // Blue
          b = center;
          g = (p[sy * src_width + sx - 1] + p[sy * src_width + sx + 1] +
               p[(sy - 1) * src_width + sx] + p[(sy + 1) * src_width + sx]) / 4;
          r = (p[(sy - 1) * src_width + sx - 1] + p[(sy - 1) * src_width + sx + 1] +
               p[(sy + 1) * src_width + sx - 1] + p[(sy + 1) * src_width + sx + 1]) / 4;
          break;

        case 1: // Green
          g = center;
          {
            /* The two green sites of a Bayer quad have opposite neighbor
               orientations; derive the horizontal neighbor channel from the
               pattern table instead of assuming an anti-diagonal (RGGB/BGGR)
               green layout. With greens on the main diagonal (GRBG/GBRG) the
               fixed assignment swapped R and B on half of the green sites. */
            int havg = (p[sy * src_width + sx - 1] + p[sy * src_width + sx + 1]) / 2;
            int vavg = (p[(sy - 1) * src_width + sx] + p[(sy + 1) * src_width + sx]) / 2;
            if (offsets[row_parity][col_parity ^ 1] == 2) { // horizontal neighbors are red
              r = havg;
              b = vavg;
            } else {
              r = vavg;
              b = havg;
            }
          }
          break;

        case 2: // Red
          r = center;
          g = (p[sy * src_width + sx - 1] + p[sy * src_width + sx + 1] +
               p[(sy - 1) * src_width + sx] + p[(sy + 1) * src_width + sx]) / 4;
          b = (p[(sy - 1) * src_width + sx - 1] + p[(sy - 1) * src_width + sx + 1] +
               p[(sy + 1) * src_width + sx - 1] + p[(sy + 1) * src_width + sx + 1]) / 4;
          break;
      }

      int dst_idx = (dy * dst_width + dx) * 3;
      dst_rgb[dst_idx + 0] = (uint8_t)clamp(r, 0, 255);
      dst_rgb[dst_idx + 1] = (uint8_t)clamp(g, 0, 255);
      dst_rgb[dst_idx + 2] = (uint8_t)clamp(b, 0, 255);
    }
  }
}

#define FP_SHIFT 16
#define FP_ONE   (1 << FP_SHIFT)
#define FP_MASK  (FP_ONE - 1)

static void unpack_pixel(const uint8_t *buf, image_format_t format, int *r, int *g, int *b) {
  switch (format) {
    case IMAGE_FORMAT_GRAYSCALE:
      *r = *g = *b = buf[0];
      break;
    case IMAGE_FORMAT_RGB565: {
      uint16_t px = buf[0] | (buf[1] << 8);
      *r = ((px >> 11) & 0x1F) << 3;
      *g = ((px >> 5)  & 0x3F) << 2;
      *b = (px & 0x1F) << 3;
      break;
    }
    case IMAGE_FORMAT_RGB888:
      *r = buf[0];
      *g = buf[1];
      *b = buf[2];
      break;
  }
}

static void pack_pixel(uint8_t *buf, image_format_t format, int r, int g, int b) {
  switch (format) {
    case IMAGE_FORMAT_GRAYSCALE:
      buf[0] = (uint8_t)((r * 299 + g * 587 + b * 114) / 1000); // luminance
      break;
    case IMAGE_FORMAT_RGB565: {
      uint16_t px = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
      buf[0] = px & 0xFF;
      buf[1] = (px >> 8) & 0xFF;
      break;
    }
    case IMAGE_FORMAT_RGB888:
      buf[0] = (uint8_t)r;
      buf[1] = (uint8_t)g;
      buf[2] = (uint8_t)b;
      break;
  }
}

void crop_resize_rgb565_to_rgb888(
    const uint8_t *src,
    int src_width,
    int src_height,
    uint8_t *dst,
    int dst_width,
    int dst_height)
{
    int crop_size = src_height;                 // 720 for 1280x720
    int crop_x = (src_width - crop_size) / 2;   // center horizontally
    int crop_y = 0;

    int x_ratio = ((crop_size - 1) << FP_SHIFT) / (dst_width - 1);
    int y_ratio = ((crop_size - 1) << FP_SHIFT) / (dst_height - 1);

    for (int y = 0; y < dst_height; y++) {
        int src_y_fp = y * y_ratio;
        int sy = (src_y_fp >> FP_SHIFT) + crop_y;

        for (int x = 0; x < dst_width; x++) {
            int src_x_fp = x * x_ratio;
            int sx = (src_x_fp >> FP_SHIFT) + crop_x;

            int idx = (sy * src_width + sx) * 2;
            uint16_t pixel = src[idx] | (src[idx + 1] << 8);

            uint8_t r5 = (pixel >> 11) & 0x1F;
            uint8_t g6 = (pixel >> 5)  & 0x3F;
            uint8_t b5 = pixel & 0x1F;

            uint8_t *dst_pixel = &dst[(y * dst_width + x) * 3];

            dst_pixel[0] = (r5 << 3) | (r5 >> 2);
            dst_pixel[1] = (g6 << 2) | (g6 >> 4);
            dst_pixel[2] = (b5 << 3) | (b5 >> 2);
        }
    }
}

void crop_resize_rgb888_to_rgb888(
    const uint8_t *src,
    int src_width,
    int src_height,
    uint8_t *dst,
    int dst_width,
    int dst_height)
{
    int crop_size = src_height;                 // 480 for 640x480
    int crop_x = (src_width - crop_size) / 2;   // center horizontally
    int crop_y = 0;

    int x_ratio = ((crop_size - 1) << FP_SHIFT) / (dst_width - 1);
    int y_ratio = ((crop_size - 1) << FP_SHIFT) / (dst_height - 1);

    for (int y = 0; y < dst_height; y++) {
        int src_y_fp = y * y_ratio;
        int sy = (src_y_fp >> FP_SHIFT) + crop_y;

        for (int x = 0; x < dst_width; x++) {
            int src_x_fp = x * x_ratio;
            int sx = (src_x_fp >> FP_SHIFT) + crop_x;

            const uint8_t *src_pixel = &src[(sy * src_width + sx) * 3];
            uint8_t *dst_pixel = &dst[(y * dst_width + x) * 3];

            dst_pixel[0] = src_pixel[0];
            dst_pixel[1] = src_pixel[1];
            dst_pixel[2] = src_pixel[2];
        }
    }
}

__WEAK void image_resize(const uint8_t *src,
                         int src_width,
                         int src_height,
                         uint8_t *dst,
                         int dst_width,
                         int dst_height,
                         image_format_t src_format,
                         image_format_t dst_format) {
  int src_bpp = (src_format == IMAGE_FORMAT_GRAYSCALE) ? 1 :
                (src_format == IMAGE_FORMAT_RGB565)    ? 2 : 3;
  int dst_bpp = (dst_format == IMAGE_FORMAT_GRAYSCALE) ? 1 :
                (dst_format == IMAGE_FORMAT_RGB565)    ? 2 : 3;

  int x_ratio = ((src_width - 1) << FP_SHIFT) / (dst_width - 1);
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

      int r00, g00, b00;
      int r01, g01, b01;
      int r10, g10, b10;
      int r11, g11, b11;

      const uint8_t *p00 = &src[(y0 * src_width + x0) * src_bpp];
      const uint8_t *p01 = &src[(y0 * src_width + x1) * src_bpp];
      const uint8_t *p10 = &src[(y1 * src_width + x0) * src_bpp];
      const uint8_t *p11 = &src[(y1 * src_width + x1) * src_bpp];

      unpack_pixel(p00, src_format, &r00, &g00, &b00);
      unpack_pixel(p01, src_format, &r01, &g01, &b01);
      unpack_pixel(p10, src_format, &r10, &g10, &b10);
      unpack_pixel(p11, src_format, &r11, &g11, &b11);

      // Interpolate each channel
      int r_top = ((FP_ONE - wx) * r00 + wx * r01) >> FP_SHIFT;
      int r_bot = ((FP_ONE - wx) * r10 + wx * r11) >> FP_SHIFT;
      int r = ((FP_ONE - wy) * r_top + wy * r_bot) >> FP_SHIFT;

      int g_top = ((FP_ONE - wx) * g00 + wx * g01) >> FP_SHIFT;
      int g_bot = ((FP_ONE - wx) * g10 + wx * g11) >> FP_SHIFT;
      int g = ((FP_ONE - wy) * g_top + wy * g_bot) >> FP_SHIFT;

      int b_top = ((FP_ONE - wx) * b00 + wx * b01) >> FP_SHIFT;
      int b_bot = ((FP_ONE - wx) * b10 + wx * b11) >> FP_SHIFT;
      int b = ((FP_ONE - wy) * b_top + wy * b_bot) >> FP_SHIFT;

      uint8_t *dst_pixel = &dst[(y * dst_width + x) * dst_bpp];
      pack_pixel(dst_pixel, dst_format, r, g, b);
    }
  }
}

__WEAK void FastResizeRgb888ToWindow(const uint8_t* src, uint32_t src_width,
                                     uint32_t src_height, uint8_t* dst,
                                     uint32_t dst_fb_width,
                                     uint32_t dst_win_x,
                                     uint32_t dst_win_y,
                                     uint32_t dst_win_width,
                                     uint32_t dst_win_height,
                                     int flip_horizontal,
                                     int flip_vertical,
                                     int swap_rb) {
  const uint32_t bytes_per_pixel_rgb888 = 3U;
  if ((src == NULL) || (dst == NULL) ||
      (src_width == 0U) || (src_height == 0U) ||
      (dst_win_width == 0U) || (dst_win_height == 0U)) {
    return;
  }

  for (uint32_t y = 0; y < dst_win_height; y++) {
      const uint32_t y_sample = (flip_vertical != 0) ?
                                (dst_win_height - 1U - y) : y;
      uint32_t sy = (y_sample * src_height) / dst_win_height;
      if (sy >= src_height) {
          sy = src_height - 1U;
      }
      const uint8_t* src_row =
          src + (sy * src_width * bytes_per_pixel_rgb888);
      uint8_t* dst_row = dst +
          (((y + dst_win_y) * dst_fb_width + dst_win_x) *
            bytes_per_pixel_rgb888);

      for (uint32_t x = 0; x < dst_win_width; x++) {
          const uint32_t x_sample = (flip_horizontal != 0) ?
                                    (dst_win_width - 1U - x) : x;
          uint32_t sx = (x_sample * src_width) / dst_win_width;
          if (sx >= src_width) {
              sx = src_width - 1U;
          }
          const uint8_t* s = src_row + (sx * bytes_per_pixel_rgb888);
          uint8_t* d = dst_row + (x * bytes_per_pixel_rgb888);
          d[0] = (swap_rb != 0) ? s[2] : s[0];
          d[1] = s[1];
          d[2] = (swap_rb != 0) ? s[0] : s[2];
      }
  }
}

__WEAK void image_copy_to_framebuffer(const uint8_t *src,
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
                                      int swap_rb) {
  int bpp;

  switch (format) {
    case IMAGE_FORMAT_GRAYSCALE:
      bpp = 1;
      break;
    case IMAGE_FORMAT_RGB565:
      bpp = 2;
      break;
    case IMAGE_FORMAT_RGB888:
      bpp = 3;
      break;
    default:
      return; // unsupported format
  }

  for (int y = 0; y < src_height; ++y) {
    int dst_y = y + y_offset;
    if (dst_y < 0 || dst_y >= dst_height)
      continue;

    for (int x = 0; x < src_width; ++x) {
      int dst_x = x + x_offset;
      if (dst_x < 0 || dst_x >= dst_width)
        continue;

      int src_y = flip_vertical ? (src_height - 1 - y) : y;
      int src_x = flip_horizontal ? (src_width - 1 - x) : x;
      int src_idx = (src_y * src_width + src_x) * bpp;
      int dst_idx = (dst_y * dst_width + dst_x) * bpp;

      if ((format == IMAGE_FORMAT_RGB888) && (swap_rb != 0)) {
        dst[dst_idx + 0] = src[src_idx + 2];
        dst[dst_idx + 1] = src[src_idx + 1];
        dst[dst_idx + 2] = src[src_idx + 0];
      } else {
        for (int i = 0; i < bpp; ++i) {
          dst[dst_idx + i] = src[src_idx + i];
        }
      }
    }
  }
}

__WEAK void convert_rgb565_to_rgb888(const uint8_t *src,
                                     uint8_t *dst,
                                     int width,
                                     int height) {
  for (int i = 0; i < width * height; ++i) {
    uint16_t pixel = src[2 * i] | (src[2 * i + 1] << 8);

    /* Extract RGB components from RGB565 */
    uint8_t r5 = (pixel >> 11) & 0x1F;
    uint8_t g6 = (pixel >> 5) & 0x3F;
    uint8_t b5 = pixel & 0x1F;

    /* Convert to 8-bit RGB888 */
    uint8_t r8 = (r5 << 3) | (r5 >> 2);  // replicate upper bits
    uint8_t g8 = (g6 << 2) | (g6 >> 4);
    uint8_t b8 = (b5 << 3) | (b5 >> 2);

    dst[3 * i + 0] = r8;
    dst[3 * i + 1] = g8;
    dst[3 * i + 2] = b8;
  }
}

__WEAK void crop_rgb565_to_rgb888(const uint8_t *src,
                                  int src_width,
                                  int src_height,
                                  uint8_t *dst,
                                  int crop_x,
                                  int crop_y,
                                  int crop_width,
                                  int crop_height) {
  for (int y = 0; y < crop_height; ++y) {
    int src_y = crop_y + y;
    if (src_y >= src_height) break;

    for (int x = 0; x < crop_width; ++x) {
      int src_x = crop_x + x;
      if (src_x >= src_width) break;

      int src_idx = (src_y * src_width + src_x) * 2;
      uint16_t pixel = src[src_idx] | (src[src_idx + 1] << 8);

      // Extract RGB565 components
      uint8_t r5 = (pixel >> 11) & 0x1F;
      uint8_t g6 = (pixel >> 5)  & 0x3F;
      uint8_t b5 = pixel & 0x1F;

      // Convert to RGB888
      uint8_t r8 = (r5 << 3) | (r5 >> 2);
      uint8_t g8 = (g6 << 2) | (g6 >> 4);
      uint8_t b8 = (b5 << 3) | (b5 >> 2);

      int dst_idx = (y * crop_width + x) * 3;
      dst[dst_idx + 0] = r8;
      dst[dst_idx + 1] = g8;
      dst[dst_idx + 2] = b8;
    }
  }
}

__WEAK void crop_rgb888_to_rgb888(const uint8_t *src,
                                  int src_width,
                                  int src_height,
                                  uint8_t *dst,
                                  int crop_x,
                                  int crop_y,
                                  int crop_width,
                                  int crop_height)
{
  const int bpp = 3; // RGB888 = 3 bytes per pixel

  for (int y = 0; y < crop_height; ++y) {
    int src_y = crop_y + y;
    if (src_y >= src_height) break; // Prevent out-of-bounds

    const uint8_t *src_row = src + (src_y * src_width + crop_x) * bpp;
    uint8_t *dst_row = dst + (y * crop_width) * bpp;

    for (int x = 0; x < crop_width; ++x) {
      int src_x = x;
      if ((crop_x + src_x) >= src_width) break;

      const uint8_t *src_pixel = src_row + src_x * bpp;
      uint8_t *dst_pixel = dst_row + x * bpp;

      dst_pixel[0] = src_pixel[0]; // R
      dst_pixel[1] = src_pixel[1]; // G
      dst_pixel[2] = src_pixel[2]; // B
    }
  }
}
