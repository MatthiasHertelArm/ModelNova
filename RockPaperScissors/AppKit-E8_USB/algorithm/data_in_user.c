/*
 * Copyright (c) 2025-2026 Arm Limited. All rights reserved.
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
 */

#include "data_in.h"

#ifndef  SIMULATOR                      // If hardware target is selected

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "cmsis_os2.h"
#include "cmsis_vstream.h"
#include "sds.h"
#include "sds_main.h"
#include "algorithm_config.h"
#include "app_setup.h"
#include "image_processing_func.h"
#include "soft_isp.h"

#ifdef   USE_SEGGER_SYSVIEW
#include "SEGGER_SYSVIEW.h"
#include "sysview_markers.h"
#endif

/* Reference to the underlying CMSIS vStream VideoIn driver */
extern vStreamDriver_t          Driver_vStreamVideoIn;
#define vStream_VideoIn       (&Driver_vStreamVideoIn)

/* Reference to the underlying CMSIS vStream VideoOut driver */
extern vStreamDriver_t          Driver_vStreamVideoOut;
#define vStream_VideoOut      (&Driver_vStreamVideoOut)

/* Camera frame buffer (RAW8, RGB565 or RGB888) */
static uint8_t CAM_Frame[CAMERA_FRAME_SIZE] CAMERA_FRAME_BUF_ATTRIBUTE;

#if (CAMERA_FRAME_TYPE == CAMERA_FRAME_TYPE_RAW8)
/* Software auto exposure for the RAW capture path: the OV5675 internal AEC
   is disabled (it converges far too dark), so drive the sensor exposure
   time and analog gain from the measured frame brightness through the CPI
   driver's sensor control passthrough. */
#include "Driver_CPI.h"
extern ARM_DRIVER_CPI Driver_CPI;

/* Sensor AE aims for the same target as the soft ISP digital stage: with a
   single authority the digital gain settles at 1.0x whenever the sensor can
   reach the target, and only lifts in light-starved scenes. Two different
   targets make the loops disagree permanently and the image pump. */
#define CAM_AE_TARGET     ((uint32_t)CAMERA_AE_TARGET)
#define CAM_AE_EXP_MAX    1900U     /* lines; sensor VTS is 2000           */
#define CAM_AE_EXP_MIN    8U        /* lines                               */
/* Anti-flicker: one 100 Hz mains half-period in sensor lines (HTS 750 at
   90 MHz sensor clock = 8.33 us per line -> 10 ms = 1200 lines). Holding
   the exposure at exactly this value makes mains flicker integrate to a
   constant and averages out PWM-dimmed LED lamps; analog gain does the
   brightness fine-tuning instead. VTS (2000) has room for only this one
   flicker quantum. */
#define CAM_AE_FLICKER_LINES 1200U
#define CAM_AE_GAIN_MIN   0x10000U  /* 1x, Q16.16                          */
#define CAM_AE_GAIN_MAX   0xF8000U  /* 15.5x, Q16.16 (sensor max is 15.9x) */

static void CameraAEUpdate (uint32_t mean_linear) {
  /* Register-table defaults: 1024 lines exposure (reg 512 x2), 6x gain */
  static uint32_t exp_lines = 1024U;
  static uint32_t gain_q16  = 0x60000U;
  static uint32_t frame_cnt = 0U;

  /* Sensor register writes take effect one to two frames later; adjusting
     every frame chases stale measurements and oscillates. */
  frame_cnt++;
  if ((frame_cnt % 3U) != 0U) {
    return;
  }

  if (mean_linear == 0U) {
    mean_linear = 1U;
  }

  /* Ratio to target in Q8; +-10% dead zone, slew-limited per frame */
  uint32_t ratio_q8 = (CAM_AE_TARGET << 8) / mean_linear;
  if ((ratio_q8 > 230U) && (ratio_q8 < 282U)) {
    return;
  }
  if (ratio_q8 < 205U) {
    ratio_q8 = 205U;    /* at most 0.8x down per step  */
  }
  if (ratio_q8 > 320U) {
    ratio_q8 = 320U;    /* at most 1.25x up per step   */
  }

  /* Scale the total exposure, then split: exposure time first, analog
     gain for the remainder. Whenever the scene needs at least one flicker
     period of exposure, pin the exposure to exactly that period and put
     the remainder into gain - arbitrary exposure times beat against
     flickering artificial light and produce rolling bands. */
  uint64_t total = ((uint64_t)exp_lines * gain_q16 * ratio_q8) >> 8;
  uint64_t lines = total / CAM_AE_GAIN_MIN;
  if (lines >= CAM_AE_FLICKER_LINES) {
    lines = CAM_AE_FLICKER_LINES;
  }
  if (lines > CAM_AE_EXP_MAX) {
    lines = CAM_AE_EXP_MAX;
  }
  if (lines < CAM_AE_EXP_MIN) {
    lines = CAM_AE_EXP_MIN;
  }
  uint32_t gain = (uint32_t)(total / lines);
  if (gain < CAM_AE_GAIN_MIN) {
    gain = CAM_AE_GAIN_MIN;
  }
  if (gain > CAM_AE_GAIN_MAX) {
    gain = CAM_AE_GAIN_MAX;
  }

  exp_lines = (uint32_t)lines;
  gain_q16  = gain;

  (void)Driver_CPI.Control(CPI_ISP_CAMERA_SENSOR_EXPOSURE, exp_lines);
  (void)Driver_CPI.Control(CPI_ISP_CAMERA_SENSOR_GAIN, gain_q16);
}
#endif /* CAMERA_FRAME_TYPE_RAW8 */

/* Set while a single-shot capture is in flight (pipelined with processing) */
static uint8_t capture_pending = 0U;

/* Algorithm thread ID */
osThreadId_t tid_algo = NULL;

/* Video In Stream Event Callback */
void VideoIn_Event_Callback (uint32_t event) {

  if (event & VSTREAM_EVENT_DATA) {
    /* Video frame is available in camera frame buffer */
    osThreadFlagsSet(tid_algo, 0x01U);
  }
}

/* Video Out Stream Event Callback */
void VideoOut_Event_Callback (uint32_t event) {

  if (event & VSTREAM_EVENT_DATA) {
    /* Display frame output started; single mode deactivates after this.
       Wake the algorithm thread instead of letting it busy-poll GetStatus */
    osThreadFlagsSet(tid_algo, 0x02U);
  }
}

/**
  \fn           int32_t InitInputData (void)
  \brief        Initialize system for acquiring input data.
  \return       0 on success; -1 on error
*/
int32_t InitInputData (void) {

#ifdef  USE_SEGGER_SYSVIEW
  // Set an initial marker with ID 0xFF to initialize marker tracking. This marker is ignored by SystemView.
  SEGGER_SYSVIEW_NameMarker(0xFFU,                        "Reserved");

  SEGGER_SYSVIEW_NameMarker(SYSVIEW_MARKER_INPUT_DATA,    "Input Data");
  SEGGER_SYSVIEW_NameMarker(SYSVIEW_MARKER_CAPTURE_IMAGE, "Capture Image");
  SEGGER_SYSVIEW_NameMarker(SYSVIEW_MARKER_CONVERT_IMAGE, "Convert Image");
  SEGGER_SYSVIEW_NameMarker(SYSVIEW_MARKER_RESIZE_IMAGE,  "Resize Image");
#endif

  tid_algo = osThreadGetId();

  /* Initialize Video Input Stream */
  if (vStream_VideoIn->Initialize(VideoIn_Event_Callback) != VSTREAM_OK) {
    SDS_PRINTF("Failed to initialise video input driver\n");
    return -1;
  }

  /* Set Input Video buffer */
  if (vStream_VideoIn->SetBuf(CAM_Frame, sizeof(CAM_Frame), CAMERA_FRAME_SIZE) != VSTREAM_OK) {
    SDS_PRINTF("Failed to set buffer for video input\n");
    return -1;
  }

  return 0;
}

/**
  \fn           void DiscardInputData (void)
  \brief        Discard input data.
*/
void DiscardInputData (void) {

  /* Check for new video input frame */
  uint32_t flags = osThreadFlagsWait(0x01U, osFlagsWaitAny, 0U);

  if (((flags & osFlagsError) == 0U) && // If not an error and
      ((flags & 0x01U)        != 0U)) { // if flag is set

    /* Release video input frame; do not restart capture (e.g. playback mode) */
    if (vStream_VideoIn->ReleaseBlock() != VSTREAM_OK) {
      SDS_PRINTF("Failed to release video input frame\n");
    }
    capture_pending = 0U;
  }
}

/**
  \fn           int32_t GetInputData (uint8_t *buf, uint32_t max_len)
  \brief        Get input data block as required for algorithm under test.
  \details      Size of this block has to match size expected by algorithm under test.
  \param[out]   buf             pointer to memory buffer for acquiring input data
  \param[in]    max_len         maximum number of bytes of input data to acquire
  \return       number of data bytes returned; -1 on error
*/
int32_t GetInputData (uint8_t *buf, uint32_t max_len) {
  int32_t  ret;
  uint8_t *inFrame;

  // Check input parameters
  if ((buf == NULL) || (max_len == 0U)) {
    return -1;
  }

  // Check if buffer can fit expected data
  if (max_len < ALGO_DATA_IN_BLOCK_SIZE) {
    return -1;
  }

#ifdef USE_SEGGER_SYSVIEW
  SEGGER_SYSVIEW_MarkStart(SYSVIEW_MARKER_INPUT_DATA);
#endif

#ifdef USE_SEGGER_SYSVIEW
  SEGGER_SYSVIEW_MarkStart(SYSVIEW_MARKER_CAPTURE_IMAGE);
#endif

  /* Start video capture unless one is already in flight (pipelined mode:
     the capture for this frame was started at the end of the previous call
     and has been running concurrently with inference and display) */
  if (capture_pending == 0U) {
    if (vStream_VideoIn->Start(VSTREAM_MODE_SINGLE) != VSTREAM_OK) {
      SDS_PRINTF("Failed to start video capture\n");
      return -1;
    }
    capture_pending = 1U;
  }

  /* Wait for new video input frame */
  {
    extern volatile uint32_t cam_wait_ms_total;   /* wait-budget telemetry */
    uint32_t t0 = osKernelGetTickCount();
    osThreadFlagsWait(0x01U, osFlagsWaitAny, osWaitForever);
    cam_wait_ms_total += osKernelGetTickCount() - t0;
  }
  capture_pending = 0U;

    /* Get input video frame buffer */
  inFrame = (uint8_t *)vStream_VideoIn->GetBlock();
  if (inFrame == NULL) {
    /* Ring wedged with the app owning the block (e.g. a startup race left a
       GetBlock without its ReleaseBlock): release and take the next frame */
    (void)vStream_VideoIn->ReleaseBlock();
    inFrame = (uint8_t *)vStream_VideoIn->GetBlock();
  }
  if (inFrame == NULL) {
    SDS_PRINTF("Failed to get video input frame\n");
    return -1;
  }

  if ((record_camera != 0U) && (sds_state == SDS_STATE_ACTIVE)) {
    // If recording of images captured by camera is active, use image recording timeslot
    // as reference timeslot for algorithm input and output data
    timeslot = osKernelGetTickCount();

    // Record raw captured image
    do {
      ret = sdsWrite(sds_camera_id, timeslot, CAM_Frame, sizeof(CAM_Frame));
      if (ret == SDS_NO_SPACE) {
        osDelay(10U);
      }
    } while (ret == SDS_NO_SPACE);
    SDS_ASSERT(ret == sizeof(CAM_Frame));
  }

#ifdef USE_SEGGER_SYSVIEW
  SEGGER_SYSVIEW_MarkStop(SYSVIEW_MARKER_CAPTURE_IMAGE);
#endif

#ifdef USE_SEGGER_SYSVIEW
  SEGGER_SYSVIEW_MarkStart(SYSVIEW_MARKER_CONVERT_IMAGE);
#endif

#ifdef USE_SEGGER_SYSVIEW
  SEGGER_SYSVIEW_MarkStop(SYSVIEW_MARKER_CONVERT_IMAGE);
#endif

#ifdef USE_SEGGER_SYSVIEW
  SEGGER_SYSVIEW_MarkStart(SYSVIEW_MARKER_RESIZE_IMAGE);
#endif

  /* Center-crop and resize camera frame to fit ML model expected size */
#if (CAMERA_FRAME_TYPE == CAMERA_FRAME_TYPE_RAW8)
  /* RAW8 Bayer frame (e.g. OV5675 without ISP): center-square crop with
     bilinear debayering and scaling in one pass */
  crop_and_debayer(inFrame,
                   CAMERA_FRAME_WIDTH,
                   CAMERA_FRAME_HEIGHT,
                   (CAMERA_FRAME_WIDTH - CAMERA_FRAME_HEIGHT) / 2,
                   0,
                   buf,
                   ML_IMAGE_WIDTH,
                   ML_IMAGE_HEIGHT,
                   (bayer_pattern_t)CAMERA_FRAME_BAYER);
  /* Raw sensor data has a black-level pedestal, no white balance (Bayer
     green dominates), desaturated colors and radial red lens shading;
     run the software ISP, and feed the measured (center-weighted) scene
     brightness into the sensor exposure loop */
  {
    static const uint16_t   lsc_r_q8[] = CAMERA_LSC_R_GAIN_Q8;
    static const SoftISP_Config isp_cfg = {
      .black_level         = CAMERA_BLACK_LEVEL,
      .saturation_q8       = CAMERA_SATURATION_Q8,
      .lsc_r_q8            = lsc_r_q8,
      .lsc_r_len           = (int)(sizeof(lsc_r_q8) / sizeof(lsc_r_q8[0])),
      .ae_target           = CAMERA_AE_TARGET,
      .ae_center_weight_q8 = CAMERA_AE_CENTER_WEIGHT_Q8,
      .awb_min_gray_pct    = CAMERA_AWB_MIN_GRAY_PCT,
      .sharpen_q8          = CAMERA_SHARPEN_Q8,
    };
    SoftISP_Status isp_st;

    SoftISP_Process(buf, ML_IMAGE_WIDTH, ML_IMAGE_HEIGHT, &isp_cfg, &isp_st);
    CameraAEUpdate(isp_st.mean_linear);
  }
#elif (CAMERA_FRAME_TYPE == CAMERA_FRAME_TYPE_RGB888)
  crop_resize_rgb888_to_rgb888(inFrame,
                               CAMERA_FRAME_WIDTH,
                               CAMERA_FRAME_HEIGHT,
                               buf,
                               ML_IMAGE_WIDTH,
                               ML_IMAGE_HEIGHT);
#else
  crop_resize_rgb565_to_rgb888(inFrame,
                               CAMERA_FRAME_WIDTH,
                               CAMERA_FRAME_HEIGHT,
                               buf,
                               ML_IMAGE_WIDTH,
                               ML_IMAGE_HEIGHT);
#endif

#ifdef USE_SEGGER_SYSVIEW
  SEGGER_SYSVIEW_MarkStop(SYSVIEW_MARKER_RESIZE_IMAGE);
#endif

  /* Release input frame */
  if (vStream_VideoIn->ReleaseBlock() != VSTREAM_OK) {
    SDS_PRINTF("Failed to release video input frame\n");
  }

  /* Prime the next capture so the sensor and DMA run concurrently with
     inference and display of the frame just returned in buf */
  if (vStream_VideoIn->Start(VSTREAM_MODE_SINGLE) == VSTREAM_OK) {
    capture_pending = 1U;
  }

#ifdef USE_SEGGER_SYSVIEW
  SEGGER_SYSVIEW_MarkStop(SYSVIEW_MARKER_INPUT_DATA);
#endif

  return ALGO_DATA_IN_BLOCK_SIZE;
}

#else                                   // If simulator target is selected
int32_t InitInputData    (void) { return -1; }
void    DiscardInputData (void) { }
int32_t GetInputData     (uint8_t *buf, uint32_t max_len) { return -1; }
#endif