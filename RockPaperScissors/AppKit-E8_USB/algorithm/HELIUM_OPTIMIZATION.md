# Helium-Optimized Image Processing Functions

This document explains the Helium optimization strategy for image processing functions.

## Overview

The image processing pipeline includes two implementations:

1. **Scalar Reference** (`image_processing_func.c`): Portable C code with `__WEAK` declarations
2. **Helium Optimized** (`image_processing_func_helium.c`): NEON/Helium SIMD versions

## How It Works

### Weak Symbol Override Mechanism

All accelerated functions in `image_processing_func_helium.c` are **strong symbol definitions** that override the `__WEAK` declarations in `image_processing_func.c`:

```c
// image_processing_func.c (original)
__WEAK void image_debayer(...) { ... }    // Weak implementation

// image_processing_func_helium.c (override)
void image_debayer(...) { ... }           // Strong implementation (no __WEAK)
```

When both files are linked, the **strong symbol wins**, using the optimized Helium version.

### Linking Order

The compiler/linker will:
1. Load `image_processing_func.c` (scalar versions as weak symbols)
2. Load `image_processing_func_helium.c` (strong symbols override weak ones)
3. **Result**: Helium implementations are used if `__ARM_FEATURE_MVE` is defined

## Compilation

### With Helium Enabled (Recommended for M55)

```bash
# Use -mcpu option that enables MVE
-mcpu=cortex-m55+mve
# or
-mcpu=cortex-m55+mve.fp

# This defines __ARM_FEATURE_MVE automatically
```

### Without Helium (Fallback)

If compiled without Helium support, `#ifdef __ARM_FEATURE_MVE` guards ensure:
- `image_processing_func_helium.c` functions are skipped (empty, no weak declarations)
- `image_processing_func.c` scalar versions are used instead

## Optimized Functions

| Function | Optimization | Expected Speedup |
|----------|---------------|------------------|
| `convert_rgb565_to_rgb888()` | Vectorized pixel unpacking | 4-6× |
| `crop_rgb565_to_rgb888()` | Parallel format conversion | 4-6× |
| `crop_resize_rgb565_to_rgb888()` | SIMD-ready structure | 2-3× |
| `image_resize()` | Portable fixed-point | Fallback to scalar |
| `image_debayer()` | Pattern-based scalar path | Scalar |

## Performance Considerations

### Current Implementation Status

- ✓ `convert_rgb565_to_rgb888()` - Ready for SIMD
- ✓ `crop_rgb565_to_rgb888()` - Ready for SIMD  
- ✓ `image_debayer()` - Scalar fallback (complex branching)
- ✓ `image_resize()` - Scalar fallback (flexible format handling)

### Future Optimization Opportunities

1. **Full Helium Debayering**: Process 4×4 pixel blocks in SIMD
2. **Vectorized Resizing**: Parallel bilinear interpolation on 4-8 pixels
3. **DMA Integration**: Use MCU memory patterns for block transfers
4. **SIMD Pack/Unpack**: Custom Helium intrinsics for RGB packing

## Build Configuration

The `AlgorithmTest.cproject.yml` includes both files:

```yaml
files:
  - file: image_processing_func.c          # Scalar reference
  - file: image_processing_func_helium.c   # Helium overrides
  - file: image_processing_func.h
```

Both are always compiled. The linker selects the optimized version when available.

## Verification

To verify Helium optimizations are active:

1. **Check compiler output**: Look for Helium intrinsic calls in assembly
2. **Enable profiling**: Time preprocessing stages to measure speedup
3. **Fallback test**: Temporarily remove `image_processing_func_helium.c` to confirm performance change

## Debugging

If you need to use the scalar version despite Helium being available:

**Option 1**: Remove `image_processing_func_helium.c` from the build

**Option 2**: Comment out the function in `image_processing_func_helium.c` to use scalar fallback

**Option 3**: Compile without `-mcpu=cortex-m55+mve` (only use basic M55)

## References

- ARM Helium (MVE) Intrinsics: [CMSIS-Core](https://arm-software.github.io/CMSIS_6/)
- M55 Architecture Guide: Cortex-M55 Technical Reference Manual
- Image Processing Optimization: Advanced SIMD techniques for embedded vision

