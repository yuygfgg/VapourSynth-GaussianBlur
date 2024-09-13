# Gaussian Blur VapourSynth Plugin

This VapourSynth plugin applies a Gaussian blur to the input video node. The blur strength is determined by the `radius` parameter. The implementation uses NEON acceleration on ARM platforms for optimal performance.

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
- **16-bit Support**: Input clip must be 16-bit integer format.

