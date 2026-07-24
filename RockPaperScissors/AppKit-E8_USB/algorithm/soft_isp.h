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

#ifndef SOFT_ISP_H__
#define SOFT_ISP_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Software ISP for raw camera sensors without a usable hardware ISP.
 *
 * Processes a debayered RGB888 frame in place, applying the corrections a
 * camera ISP normally performs in hardware:
 *
 *   1. Black level subtraction and rescale to full range
 *   2. Radial lens shading correction (red channel)
 *   3. Statistics: center-weighted exposure metering, gray-world white
 *      balance measurement with a gray-pixel confidence estimate, and a
 *      luminance histogram for highlight protection
 *   4. White balance gains (smoothed; frozen when the scene contains too
 *      few near-gray pixels to trust the gray-world assumption)
 *   5. Digital exposure gain toward a mid target, capped so highlights
 *      only mildly clip
 *   6. Hue-preserving clipping (blown highlights desaturate to white
 *      instead of taking the hue of the unclipped channels)
 *   7. Saturation-restoring color matrix (gray preserving)
 *   8. sRGB gamma (OETF)
 *   9. Optional mild sharpening (post-gamma unsharp mask)
 *
 * The measured scene brightness is reported so the caller can drive the
 * sensor's exposure time / analog gain (sensor AE); the digital exposure
 * stage then acts as the fine trim on top.
 *
 * All state (smoothed gains, lookup tables) is internal and static: the
 * module processes one stream.
 */

typedef struct {
    /* Sensor / color */
    int             black_level;      /* sensor pedestal, 8-bit range (0..64) */
    int             saturation_q8;    /* color matrix strength, 256 = 1.0 (256..768) */
    const uint16_t *lsc_r_q8;         /* radial red gain table (Q8), NULL = off */
    int             lsc_r_len;        /* entries in lsc_r_q8 */

    /* Auto exposure */
    int ae_target;                    /* linear mean target (8-bit, e.g. 120) */
    int ae_center_weight_q8;          /* extra weight of the center half-box in the
                                         metering mean; 0 = full-frame average,
                                         512 = center counts 3x (Q8 relative to
                                         the full-frame weight of 256) */

    /* Auto white balance */
    int awb_min_gray_pct;             /* freeze gain adaptation when fewer than
                                         this percentage of valid pixels are
                                         near-gray under the current gains */

    /* Sharpening */
    int sharpen_q8;                   /* unsharp strength, 0 = off, 64 = 0.25 */
} SoftISP_Config;

typedef struct {
    uint32_t mean_linear;             /* center-weighted linear mean (after black
                                         level and shading correction); metering
                                         input for the sensor exposure loop */
    int      gain_r_q8;               /* current white balance gains  */
    int      gain_b_q8;
    int      gain_exp_q8;             /* current digital exposure gain */
    int      awb_gray_pct;            /* near-gray pixel percentage of the last
                                         frame (AWB confidence)        */
    int      awb_frozen;              /* nonzero if the last frame froze AWB */
} SoftISP_Status;

/**
 * Process one RGB888 frame in place.
 *
 * @param[in,out] rgb     Frame buffer, width*height*3 bytes.
 * @param[in]     width   Frame width in pixels.
 * @param[in]     height  Frame height in pixels.
 * @param[in]     cfg     Processing configuration (must not be NULL).
 * @param[out]    status  Optional: measured statistics and current gains
 *                        (may be NULL).
 */
void SoftISP_Process(uint8_t *rgb, int width, int height,
                     const SoftISP_Config *cfg, SoftISP_Status *status);

#ifdef __cplusplus
}
#endif

#endif /* SOFT_ISP_H__ */
