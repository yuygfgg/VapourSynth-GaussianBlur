# Gaussian Blur VapourSynth Plugin

This VapourSynth plugin applies a Gaussian blur to the input video node. The blur strength is determined by the `radius` parameter. The implementation uses NEON acceleration on ARM platforms for optimal performance.

**NEVER USE IT FOR ANY PURPOSE. I WROTE IT ONLY FOR LEARNING ABOUT VS PLUGINS AND NEON**

## Build Instructions

```bash
meson setup build
ninja -C build
ninja -C build install
```

## Usage

```python
import vapoursynth as vs
core = vs.core

core.std.LoadPlugin(path="/path/to/libgaussianblur.dylib")
clip = core.ffms2.Source("input_video.mp4")

# Apply Gaussian Blur with radius 5
blurred = core.gaussblur.GaussianBlur(clip=clip, radius=5)
blurred.set_output()
```

## Notes

- **NEON Acceleration**: Enabled by default on ARM platforms.

NEON and C code do not always have the same output because of rounding issues, but the differences are negligible, as the following output shows:

```
Number of different pixels: 256521
Total sum of absolute pixel differences: 269677

Position and values of the top 5 pixels with the largest differences:
Position 1: (1054, 769)
  img1 pixel value: [27 35 35]
  img2 pixel value: [26 34 36]
  Difference value: 3
Position 2: (550, 1565)
  img1 pixel value: [217 218 221]
  img2 pixel value: [216 217 222]
  Difference value: 3
Position 3: (1054, 768)
  img1 pixel value: [26 34 36]
  img2 pixel value: [27 35 35]
  Difference value: 3
Position 4: (505, 1517)
  img1 pixel value: [246 247 248]
  img2 pixel value: [247 248 249]
  Difference value: 3
Position 5: (212, 1446)
  img1 pixel value: [250 251 250]
  img2 pixel value: [249 250 251]
  Difference value: 3
```

- **16-bit Support**: Input clip must be 16-bit integer format.


## Performance

Input clip: $1920$ $\times$ $1080$, YUV420P16, radius=100

| Configuration  | FPS    |
|----------------|--------|
| C+O0           | 1.85   |
| C+O3           | 6.72   |
| C+Ofast        | 8.6    |
| Neon+O0        | 2.79   |
| Neon+O3        | 25.7   |
| Neon+Ofast     | 28.96  |

**Compiler**: clang-1500.3.9.4  
**CPU**: Apple M2pro
