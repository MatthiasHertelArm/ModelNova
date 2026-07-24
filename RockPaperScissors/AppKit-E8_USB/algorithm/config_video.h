/*
 * SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its
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

#ifndef CONFIG_VIDEO_H__
#define CONFIG_VIDEO_H__

//-------- <<< Use Configuration Wizard in Context Menu >>> --------------------

// <h>Camera Configuration
// =======================

//  <o>Camera Frame Width
//  <i> Define the camera frame width.
//  <i> ISP path: the ISP crops a centered square from the sensor and scales
//  <i> it to this size.
//  <i> Default: 480
#ifndef CAMERA_FRAME_WIDTH
#define CAMERA_FRAME_WIDTH          640
#endif

//  <o>Camera Frame Height
//  <i> Define the camera frame height.
//  <i> Default: 480
#ifndef CAMERA_FRAME_HEIGHT
#define CAMERA_FRAME_HEIGHT         480
#endif

//  <o>Frame Type <0=>RAW8 <1=>RGB565 <2=>RGB888
//  <i> Define whether camera frame is raw or RGB.
//  <i> RAW8: OV5675 captured directly by the CPI delivers RAW8 Bayer frames
//  <i> (the application debayers them); RGB888 applies when the ISP path
//  <i> is enabled (RTE_CPI_ISP_PORT).
//  <i> Default: 0
#ifndef CAMERA_FRAME_TYPE
#define CAMERA_FRAME_TYPE           0
#endif

//  <o>Frame Bayer Pattern <0=>RGGB <1=>BGGR <2=>GRBG <3=>GBRG
//  <i> Define the raw camera frame Bayer pattern.
//  <i> OV5675 outputs GRBG (SGRBG10 in the Linux driver; the Bayer order is
//  <i> not affected by the sensor flip controls).
//  <i> Default: 2
#ifndef CAMERA_FRAME_BAYER
#define CAMERA_FRAME_BAYER          2
#endif

//  <h>Lens Shading Correction (RAW capture path)
//  <i> Radial red-channel gain table in Q8 (256 = 1.0), indexed by squared
//  <i> distance from the image center (last entry = image corner). The
//  <i> OV5675 module's IR-cut filter shifts blue at steep ray angles and
//  <i> attenuates red toward the field edges, which turns the image green
//  <i> outside the center after global white balance. The severity depends
//  <i> on the light source spectrum (narrowband red LED light is cut almost
//  <i> completely, broadband light only partially), so this table is a
//  <i> compromise calibrated from a broadband flat-field capture.
#ifndef CAMERA_LSC_R_GAIN_Q8
#define CAMERA_LSC_R_GAIN_Q8 \
  { 259, 264, 270, 277, 285, 293, 302, 313, 324, 338, 352, 369, \
    389, 411, 437, 467, 504, 548, 602, 670, 759, 878, 896, 896 }
#endif
//  </h>

//  <o>Sensor Black Level (8-bit)
//  <i> Raw sensor pedestal subtracted before white balance (RAW capture path).
//  <i> OV5675: 64 on the 10-bit scale = 16 in the captured 8 MSBs.
//  <i> Default: 16
#ifndef CAMERA_BLACK_LEVEL
#define CAMERA_BLACK_LEVEL          16
#endif

//  <o>Color Saturation (Q8)
//  <i> Strength of the saturation-restoring color matrix applied after white
//  <i> balance in the RAW capture path (256 = 1.0 = no boost).
//  <i> Compensates the channel crosstalk a hardware ISP would remove with a
//  <i> calibrated CCM.
//  <i> Default: 410 (1.6)
#ifndef CAMERA_SATURATION_Q8
#define CAMERA_SATURATION_Q8        410
#endif

//  <s>Frame Buffer Section Name
//  <i> Define the name of the camera frame buffer section.
//  <i> Default: ".bss.camera_frame_buf"
#ifndef CAMERA_FRAME_BUF_SECTION
#define CAMERA_FRAME_BUF_SECTION    ".bss.camera_frame_buf"
#endif

//  <o>Frame Buffer Alignment
//  <i> Define the camera frame buffer alignment in bytes.
//  <i> Default: 32
#ifndef CAMERA_FRAME_BUF_ALIGNMENT
#define CAMERA_FRAME_BUF_ALIGNMENT  32
#endif

//  <o>RGB Image Width
//  <i> Define the RGB image width.
//  <i> Default: 384
#ifndef RGB_IMAGE_WIDTH
#define RGB_IMAGE_WIDTH             384
#endif

//  <o>RGB Image Height
//  <i> Define the RGB image height.
//  <i> Default: 384
#ifndef RGB_IMAGE_HEIGHT
#define RGB_IMAGE_HEIGHT            384
#endif

//  <s>RGB Image Buffer Section Name
//  <i> Define the name of the RGB image buffer section.
//  <i> Default: ".bss.rgb_image_buf"
#ifndef RGB_IMAGE_BUF_SECTION
#define RGB_IMAGE_BUF_SECTION       ".bss.rgb_image_buf"
#endif

//  <o>RGB Image Buffer Alignment
//  <i>Define the RGB image buffer alignment in bytes.
//  <i>Default: 4
#ifndef RGB_IMAGE_BUF_ALIGNMENT
#define RGB_IMAGE_BUF_ALIGNMENT     4
#endif

// </h>

// <h>Display Configuration
// ========================

//  <o>Display Frame Width
//  <i> Defines the display frame width.
//  <i> Common display frame widths: 480, 800, 1024, 1280.
//  <i> Default: 800
#ifndef DISPLAY_FRAME_WIDTH
#define DISPLAY_FRAME_WIDTH         480
#endif

//  <o>Display Frame Height
//  <i> Defines the display frame height.
//  <i> Common display frame heights: 320, 480, 600, 800.
//  <i> Default: 480
#ifndef DISPLAY_FRAME_HEIGHT
#define DISPLAY_FRAME_HEIGHT        800
#endif

//  <s>Frame Buffer Section Name
//  <i> Define the name of the display frame buffer section
//  <i> Default: ".bss.lcd_frame_buf"
#ifndef DISPLAY_FRAME_BUF_SECTION
#define DISPLAY_FRAME_BUF_SECTION   ".bss.lcd_frame_buf"
#endif

//  <o>Frame Buffer Alignment
//  <i> Define the display frame buffer alignment in bytes
//  <i> Default: 32
#ifndef DISPLAY_FRAME_BUF_ALIGNMENT
#define DISPLAY_FRAME_BUF_ALIGNMENT 32
#endif

//  <o>Display Flip Horizontal <0=>Disable <1=>Enable
//  <i> Enable to mirror display content left-right.
//  <i> Default: 1
#ifndef DISPLAY_FLIP_HORIZONTAL
#define DISPLAY_FLIP_HORIZONTAL    1
#endif

//  <o>Display Flip Vertical <0=>Disable <1=>Enable
//  <i> Enable to mirror display content top-bottom.
//  <i> Default: 1
#ifndef DISPLAY_FLIP_VERTICAL
#define DISPLAY_FLIP_VERTICAL      1
#endif

//  <o>Display Swap R/B <0=>Disable <1=>Enable
//  <i> Enable when panel output appears in BGR instead of RGB.
//  <i> Default: 1
#ifndef DISPLAY_SWAP_RB
#define DISPLAY_SWAP_RB            1
#endif

//  <o>Display Square Dimension
//  <i> Define the dimension of the largest square that can fit in the display.
#ifndef DISPLAY_SQUARE_DIM
#define DISPLAY_SQUARE_DIM         ((DISPLAY_FRAME_WIDTH < DISPLAY_FRAME_HEIGHT) ? DISPLAY_FRAME_WIDTH : DISPLAY_FRAME_HEIGHT)
#endif

// </h>

#endif /* CONFIG_VIDEO_H__ */
