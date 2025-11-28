# Installation Fixes for ROSHAMBO

This document summarizes all the changes made to fix the installation issues with ROSHAMBO on modern systems with newer CUDA versions and conda environments.

## Date
November 28, 2025

## Environment
- OS: Linux (Azure)
- Python: 3.9
- CUDA: 12.5 (conda installation)
- GPU: NVIDIA A100 80GB PCIe (Compute Capability 8.0)

## Issues Fixed

### 1. CUDA Library Path Compatibility (setup.py)

**Problem**: The `locate_cuda()` function was hardcoded to look for `lib64` directory, but conda CUDA installations use `lib` instead.

**Fix**: Modified the function to check for both `lib64` (standard CUDA) and `lib` (conda CUDA):

```python
# Check for lib64 first (standard CUDA), then lib (conda CUDA)
lib_dir = os.path.join(home, "lib64")
if not os.path.exists(lib_dir):
    lib_dir = os.path.join(home, "lib")
```

**Location**: `setup.py:51-53`

---

### 2. GPU Architecture Update (setup.py)

**Problem**: The code was configured for `sm_50` (Maxwell GPU architecture from 2014), which is no longer supported by modern CUDA toolkits (CUDA 11+).

**Fix**: Updated GPU architecture to `sm_80` (Ampere architecture) for A100 GPUs:

```python
PTXFLAGS = ["-Xcompiler", "-O2", "-arch", "sm_80", "-Xptxas", "-v"]
```

**Locations**:
- `setup.py:38` (PTXFLAGS)
- `setup.py:87-88` (cpaper.cpp compilation args - removed after simplification)

**Note**: Users with different GPUs should adjust this value:
- sm_75: Turing (RTX 20xx, T4)
- sm_80: Ampere (A100, A30)
- sm_86: Ampere (RTX 30xx, A40, A10)
- sm_89: Ada Lovelace (RTX 40xx)
- sm_90: Hopper (H100)

---

### 3. Cython Build Integration (setup.py)

**Problem**: Cython was not being invoked to convert `.pyx` files to `.cpp` files during the build process.

**Fix**: Added proper Cython integration:

```python
# Import with fallback
try:
    from Cython.Build import cythonize
    HAS_CYTHON = True
except ImportError:
    HAS_CYTHON = False

# In setup() call
ext_modules=cythonize([ext], language_level="3") if HAS_CYTHON else [ext],
setup_requires=["Cython>=0.29.0"],
```

**Locations**: `setup.py:8-12, 169, 171`

---

### 4. Added paper.cu to Sources (setup.py)

**Problem**: `paper.cu` was being included via Cython's `cdef extern from`, causing it to be compiled with g++ instead of nvcc, resulting in CUDA syntax errors.

**Fix**: Added `paper.cu` to the Extension sources list so it gets compiled separately with nvcc:

```python
sources=[
    os.path.join("roshambo", "cpaper.pyx"),
    os.path.join(PAPER_DIR, "paper.cu"),  # Added this line
    # ... other sources
]
```

**Location**: `setup.py:107`

---

### 5. Deprecated CUDA API (paper/paper.cu)

**Problem**: `cudaThreadSynchronize()` was deprecated in CUDA 10.0 and removed in newer versions.

**Fix**: Replaced with the modern equivalent:

```cpp
// Old
cudaThreadSynchronize();

// New
cudaDeviceSynchronize();
```

**Location**: `paper/paper.cu:188`

---

### 6. Created Header File for CUDA Function (paper/paper.h)

**Problem**: Cython was directly including `paper.cu` via `cdef extern from "../paper/paper.cu"`, which caused g++ to try compiling CUDA code.

**Fix**: Created a proper C++ header file declaring the external function:

```cpp
#ifndef PAPER_H
#define PAPER_H

#include <list>

namespace RDKit {
    class ROMol;
}

extern "C" float** paper(int gpuID, std::list<RDKit::ROMol*>& molecules);

#endif // PAPER_H
```

**Location**: `paper/paper.h` (new file)

---

### 7. Updated Cython Declaration (roshambo/cpaper.pyx)

**Problem**: Direct inclusion of `.cu` file in Cython caused compilation issues.

**Fix**: Changed to include the header file instead:

```python
# Old
cdef extern from "../paper/paper.cu":
    float** paper(int gpuID, list[ROMol*]& molecules)

# New
cdef extern from "../paper/paper.h":
    float** paper(int gpuID, list[ROMol*]& molecules)
```

**Location**: `roshambo/cpaper.pyx:15-16`

---

### 8. Optional IPython Import (roshambo/pharmacophore.py)

**Problem**: The code unconditionally imported `IPython.display.SVG`, which is only needed for Jupyter notebooks and not available in standard environments, causing CLI usage to fail.

**Fix**: Made the import optional with a try/except block:

```python
# Old
from IPython.display import SVG

# New
try:
    from IPython.display import SVG
except ImportError:
    SVG = None

# Later in the code
if SVG is not None:
    SVG(svg)
```

**Locations**: `roshambo/pharmacophore.py:11-14, 216-217`

---

## Installation Instructions

After these fixes, installation works with:

```bash
# Ensure CUDA environment variables are set
export CUDA_HOME=/path/to/cuda  # or conda env path
export RDBASE=/path/to/rdkit
export RDKIT_INCLUDE_DIR=$RDBASE/Code
export RDKIT_LIB_DIR=$RDBASE/lib
export RDKIT_DATA_DIR=$RDBASE/Data

# Install Cython first (if not in build environment)
pip install Cython>=0.29.0

# Install in editable mode
pip install -e .
```

## Testing

Test with the recommended command:

```bash
cd notebooks/data/basic_run
roshambo --n_confs 0 --ignore_hs --color --sort_by ComboTanimoto \
  --write_to_file --working_dir . --gpu_id 0 query.sdf dataset.sdf
```

Expected output includes timing information and generates `roshambo.csv` with similarity scores.

---

## Compatibility Notes

### GPU Architecture
The code is now set to `sm_80` for A100 GPUs. Users with different GPUs must update `setup.py:38` to match their GPU's compute capability.

### CUDA Version
These fixes are compatible with CUDA 11.0+ and CUDA 12.x. The code now uses modern CUDA APIs.

### Conda vs System CUDA
The code now works with both conda-installed CUDA (which uses `lib/`) and system-installed CUDA (which uses `lib64/`).

### Python Version
Tested with Python 3.9. Should work with Python 3.7+.

---

## Files Modified

1. `setup.py` - Build configuration fixes
2. `paper/paper.cu` - CUDA API update
3. `paper/paper.h` - New header file (created)
4. `roshambo/cpaper.pyx` - Header inclusion fix
5. `roshambo/pharmacophore.py` - Optional IPython import

---

## Backward Compatibility

These changes maintain backward compatibility:
- The conda CUDA lib path fix falls back to lib64 if available
- The IPython import is optional and doesn't break notebook usage
- The header file approach is standard C++ practice

---

## Future Improvements

Consider these potential enhancements:
1. Auto-detect GPU compute capability instead of hardcoding
2. Add CUDA version detection and conditional API usage
3. Create a `pyproject.toml` for modern Python packaging
4. Add pre-compiled wheels for common platforms
5. Make GPU architecture a build-time configuration option
