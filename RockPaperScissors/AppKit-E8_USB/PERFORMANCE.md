# Performance Improvements — Rock/Paper/Scissors on AppKit-E8

This document summarizes the performance work on the `executorch-1.3.1-perf`
branch: what was changed, which component each change affects, the measured
effect on hardware, and where the remaining frame-rate ceiling lies.

All numbers were measured on the Alif AppKit-E8-AIML (M55_HP @ 400 MHz,
Ethos-U85, Debug build, AC6) using DWT cycle counts and RTOS-tick wall time
read via the debugger, and validated with the SDS playback regression
(recorded `ML_Out` results compared per record against the committed
reference recordings).

## Summary

| Metric | Before | After |
|---|---|---|
| Frame rate (live camera) | 7.0 fps | 12.5 fps |
| CIL preprocess (224×224×3 → float tensor) | 12.9 ms | 2.55 ms |
| Inference stage wall time (incl. Ethos-U85) | 34.2 ms | 9 ms |
| Total compute per frame | ~75 ms | ~28 ms (≈35 fps capable) |
| CPU idle during streaming | ~50 % | ~70 % |

The remaining gap between the 28 ms/frame compute capability and the 80 ms
frame period is capture latency in the CPI driver (see "Remaining
bottleneck").

## Changes by component

### Application (`algorithm/`)

| Change | Effect |
|---|---|
| Pipelined camera capture: the next single-shot capture is primed as soon as the current frame is copied and released, overlapping sensor+DMA with inference and display (`data_in_user.c`) | 7.0 → 12.5 fps |
| Display wait sleeps on the VideoOut event (thread flag from the driver callback) instead of busy-polling `GetStatus()` (`algorithm_user.cpp`) | wait time becomes schedulable for the SDS/USB threads |
| `ML_Out` SDS records fixed: `classification_result_t` was 108 bytes against a 50-byte SDS block, so the size guard never passed and every recorded result was zeros; `MAX_LABEL_NAME_LENGTH` reduced to 40 (`arm_executor_runner.h`) | recorded results usable; playback comparable per record |
| Per-frame result prints gated on active SDS streaming (`arm_executor_runner.cc`) | removes 10–16 ms/frame of blocking UART writes at 115200 baud |
| Video-input self-healing: a NULL `GetBlock()` (ring left in the app-owns-all-blocks state by a startup race) releases and retries instead of failing every subsequent frame (`data_in_user.c`) | recovers instead of wedging |
| Wait-budget telemetry: `algo_frame_count`, `cam_wait_ms_total`, `disp_wait_ms_total` accumulate frame count and wait times for reading via the debugger (`algorithm_user.cpp`, `data_in_user.c`) | attributes the frame period without halt-induced distortion |

### CIL layer (`algorithm/ML/cil_layer/`)

The prebuilt `cil.a` is replaced by a clean-room source implementation
(`cil_preprocess.c`, `cil_postprocess.c`) reproducing the binary's exact
numerics, recovered from its disassembly:

- preprocess: `out = ((float)px · 1/255 + (−mean)) / std` per channel with
  the ImageNet constants, evaluated through per-channel 256-entry lookup
  tables built once with the identical operation sequence — bit-identical
  values without the per-element float divide (Helium has no vector float
  division), fused into a single pass with no arena and no intermediate
  image copies;
- postprocess: numerically stable softmax (max subtraction, `expf`,
  divide by sum) + argmax, class count from the generated `model_config.h`.

Validated bit-exact on hardware: all 116 records of the playback sequence
identical to the binary CIL's output, zero confidence delta.
Preprocess: 12.9 ms → 2.55 ms. The layer now builds for whichever toolchain
builds the solution (AC6/GCC/IAR) and is debuggable. `cil.a` remains in the
tree for reference but is no longer built.

### ExecuTorch pack (vendored, `packs/PyTorch.ExecuTorch.1.3.1-rc8/`)

`op_quantize.cpp` gains a Helium (MVE) fast path for float → int8/uint8
per-tensor quantization beside the existing NEON one: `vcvtnq_s32_f32`
(round to nearest even, matching `nearbyint` in the default FP mode), add,
clamp, narrow store. The generic path converts each element through
double/int64 arithmetic, which on Cortex-M compiles to software
float→int64 libcalls; the CPU quantize wrapper op dominated the inference
stage. Inference stage wall time: 34.2 ms → 9 ms including Ethos-U85
execution. Numerically identical; playback regression bit-exact.
Candidate for upstreaming to ExecuTorch.

### Board layer (`Board/AppKit-E8_M55_HP/`)

The `BSP:External peripherals:CAMERA Sensor MT9M114` component is replaced
by a board-local override (`MT9M114_Camera_Sensor.c`, `mt9m114_isp_param.c`)
with two changes, both candidates for the Ensemble pack:

1. **30 fps VGA timing.** The pack's CPI VGA table programs
   `line_length_pck = 10723`, `frame_length_lines = 876` at 48 MHz pixclk —
   a native sensor rate of ~5 fps. The sensor timing is ported from the
   pack's own 30 fps 640×480 MIPI configuration (full 1280×960 window
   scaled to 640×480; the image now shows the full sensor field of view).
2. **Idempotent stream start.** The sensor keeps streaming between
   single-shot CPI captures; the per-frame I2C system-state transition and
   its blocking polling loops are skipped when the stream is already up.

`profiler.h`'s `ENABLE_TIME_PROFILING` is `#ifndef`-guarded so stage timing
can be enabled with a `-D` define.

### RTE configuration (`algorithm/RTE/CMSIS/RTX_Config.h`)

ISR FIFO queue deepened 16 → 64: camera VSYNC, VideoOut and USB SDSIO
events all post thread flags from interrupt context; the default queue
overflowed under bursts (`osRtxErrorISRQueueOverflow` observed on target).

## Remaining bottleneck: CPI capture latency

With all of the above, the frame period is 80 ms (12.5 fps) while per-frame
compute is 28 ms. The wait-budget telemetry over 781 frames attributes the
difference:

- camera-frame wait: **51 ms per frame** (40.0 s of 62.4 s runtime)
- display wait: 2 ms **total** across the whole run

The CPI single-shot capture takes ~2.4 sensor frame periods from arm to
completion even with the sensor streaming at 30 fps. With a single capture
engine this sets a hard floor of ~12.6 fps regardless of application-side
scheduling. The driver's continuous mode is not a usable alternative in its
current form: the per-VSYNC frame-buffer redirect issued from the IRQ
callback is not honored reliably, and the capture stream overruns adjacent
memory (observed corrupting the ExecuTorch memory pool and the LCD frame
buffer).

Paths to ~30 fps, in order of leverage:

1. **CPI driver (Ensemble pack):** a low-latency capture-from-live-stream
   mode, or a fixed continuous mode with reliable per-frame buffer rotation.
   This is the only remaining blocker; the application pipeline is already
   ~35 fps capable.
2. **Int8-input model export (ModelNova):** eliminates the CPU quantize
   wrapper op entirely and allows an int8 LUT preprocess (byte tables,
   16-lane gathers).
3. **Display offload:** the 480×480 upscale/flip runs on the CPU
   (~10 ms/frame); the Ensemble's D/AVE 2D engine could take it.

## Reproducing the measurements

- Stage times: breakpoints at the stage boundaries in
  `ExecuteAlgorithm()`, reading `DWT->CYCCNT` (CPU-active cycles; halts
  during WFE sleep and core halt) and `osKernelGetTickCount()` (wall time)
  at each stop.
- Frame rate: sample `algo_frame_count` and `osKernelGetTickCount()` twice
  around a free-run window; both advance only while the target runs, so
  the ratio is exact regardless of debugger stops.
- Wait attribution: read `cam_wait_ms_total` / `disp_wait_ms_total` after a
  free-run window. Breakpoint-stepped walks understate waits (frames queue
  while the core is halted).
- End-to-end correctness: run the SDS playback sequence
  (`sdsio-server.py -c SDS.sdsio.yml --playback`) and compare the produced
  `ML_Out.*.p.sds` per record against the committed reference recordings.
