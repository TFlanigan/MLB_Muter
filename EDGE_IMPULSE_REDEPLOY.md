# Re-applying the ESP-NN SIMD speedup after an Edge Impulse redeploy

**Read this before downloading a new model/SDK from Edge Impulse Studio.**

The ESP32-S3 build runs inference at **~489 ms/frame** (vs ~2.4–3 s with plain
reference kernels) by enabling Espressif's ESP-NN SIMD — but only by working
around bugs where the **pioarduino GCC-14 toolchain miscompiles some of esp-nn's
hand-written S3 assembly kernels**. Those workarounds live inside the
`src/edge-impulse-sdk/` tree, which **Edge Impulse Studio overwrites every time
you re-download/redeploy**. After any redeploy you must re-apply them or the
model will go back to either ~2.4 s (slow) or "always commercial" (wrong).

This is all on branch **`tier2-esp-nn-simd`** (commit `0342932`). `main` is the
safe, un-accelerated 2.4 s build — if any of this breaks, you can always fall
back to `main`.

---

## 0. Deploy the RIGHT thing in Studio

On the Deployment page choose:
- **Deployment target:** `C++ library`
- **Inference engine:** `TensorFlow Lite` ← **NOT** "EON Compiler" (this whole
  approach requires the non-EON interpreter; EON crashes/corrupts with ESP-NN here)
- **Optimization:** `Quantized (int8)`

Unzip it over `src/` (replacing `edge-impulse-sdk/`, `tflite-model/`,
`model-parameters/`). That wipes every patch below.

> Note: the model ID in filenames (currently `986043_8`) may change on redeploy.
> Where a step names `tflite_learn_986043_8*`, substitute the new ID.

---

## 1. Fast path — restore the patched SDK files from git

If the new download uses the **same Edge Impulse SDK version** (very likely for a
re-deploy of the same project), just restore the patched SDK files from the
branch. **Do NOT restore the model files** (`tflite-model/`, `model-parameters/`) —
those must come from the new download.

```bash
git checkout tier2-esp-nn-simd -- \
  src/edge-impulse-sdk/porting/espressif/ESP-NN \
  src/edge-impulse-sdk/classifier/inferencing_engines/tflite_micro.h \
  src/edge-impulse-sdk/tensorflow/lite/micro/kernels/depthwise_conv.cc \
  src/edge-impulse-sdk/tensorflow/lite/micro/kernels/add.cc \
  src/edge-impulse-sdk/tensorflow/lite/micro/kernels/fully_connected.cc \
  src/edge-impulse-sdk/tensorflow/lite/micro/kernels/softmax.cc
```

Then do **Step 2** (arena bump — model-specific, not covered by the restore) and
**Step 4** (verify). If the build fails to compile, the SDK version changed —
fall back to the manual re-apply in **Step 3**.

---

## 2. Bump the arena size (always required — it's in the new model file)

The non-EON interpreter needs a bigger arena than Studio estimates once ESP-NN
scratch is added, and it lives in **PSRAM** (do not try to force it into internal
SRAM — that crashes; see notes at the bottom).

Edit `src/tflite-model/tflite_learn_<ID>.h` and set both arena constants to
`524288` (≈ actual usage is ~177 KB; 512 KB is generous headroom):

```c
#define EI_CLASSIFIER_TFLITE_LEARN_<ID>_ARENA_SIZE     524288
const size_t tflite_learn_<ID>_arena_size = 524288;
```

---

## 3. Manual re-apply (only if the SDK version changed and Step 1 won't compile)

`platformio.ini` is **NOT** wiped by a redeploy — its build flags should still be
correct (verify they include, under `[board_freenove_s3]`):
`-O3`, `-DEI_CLASSIFIER_TFLITE_ENABLE_ESP_NN=1`, `-DCONFIG_NN_OPTIMIZED`,
`-Isrc/edge-impulse-sdk/porting/espressif/ESP-NN/include`,
`-Isrc/edge-impulse-sdk/porting/espressif/ESP-NN/src/common`,
`build_unflags = -Os`, and **no** `-DEI_CLASSIFIER_ALLOCATION_STATIC`.

### 3a. Swap in upstream esp-nn (EI's bundled copy is older/buggier)
```bash
git clone --depth 1 https://github.com/espressif/esp-nn /tmp/esp-nn
EI=src/edge-impulse-sdk/porting/espressif/ESP-NN
rm -rf "$EI/src" "$EI/include"
cp -R /tmp/esp-nn/src "$EI/src"
cp -R /tmp/esp-nn/include "$EI/include"
find "$EI/src" -name "*esp32p4*" -delete   # P4 = RISC-V, won't compile on Xtensa S3
```

### 3b. `classifier/inferencing_engines/tflite_micro.h` — guard the arena section
Find the `#ifdef EI_CLASSIFIER_ALLOCATION_STATIC` static `tensor_arena` line and
wrap the `DEFINE_SECTION(...)` form in a guard (without it, an undefined
`EI_TENSOR_ARENA_LOCATION` dumps the arena into flash/DROM → "multiple DROM
segments" boot loop):
```c
#if defined(EI_TENSOR_ARENA_LOCATION)
    static uint8_t tensor_arena[EI_CLASSIFIER_TFLITE_LARGEST_ARENA_SIZE] ALIGN(16) DEFINE_SECTION(STRINGIZE_VALUE_OF(EI_TENSOR_ARENA_LOCATION));
#else
    static uint8_t tensor_arena[EI_CLASSIFIER_TFLITE_LARGEST_ARENA_SIZE] ALIGN(16);
#endif
```
(Only matters if you ever set `EI_CLASSIFIER_ALLOCATION_STATIC` — we don't, but
keep the guard so a future attempt doesn't boot-loop.)

### 3c. `tensorflow/lite/micro/kernels/depthwise_conv.cc` — use esp-nn ANSI depthwise
The optimized S3 depthwise kernel miscomputes (~1018/1024 wrong); its plain-C
`_ansi` path is correct AND faster than TFLite reference. Right after the
`#include ".../esp_nn.h"` (inside `#if ESP_NN`), add:
```c
#undef esp_nn_depthwise_conv_s8
#define esp_nn_depthwise_conv_s8 esp_nn_depthwise_conv_s8_ansi
#undef esp_nn_get_depthwise_conv_scratch_size
#define esp_nn_get_depthwise_conv_scratch_size esp_nn_get_depthwise_conv_scratch_size_ansi
#undef esp_nn_set_depthwise_conv_scratch_buf
#define esp_nn_set_depthwise_conv_scratch_buf esp_nn_set_depthwise_conv_scratch_buf_ansi
```
Leave the int8 eval on the esp-nn path (`#if ESP_NN`, calling
`EvalQuantizedPerChannel`). The remap routes it to the correct `_ansi` kernel.
(If `_ansi` ever regresses, change that `#if ESP_NN` to `#if 0` to fall back to
TFLite reference — correct but slower.)

### 3d. `add.cc` and `fully_connected.cc` — force TFLite reference
esp-nn's ADD and FULLY_CONNECTED (both opt and ansi) diverge from TFLite
reference here and bias the model. In the **int8** case of each, change the
`#if ESP_NN` that guards the esp-nn eval branch to `#if 0`, so the `#else`
`reference_integer_ops::...` path runs.
- `add.cc`: the `#if ESP_NN` just before `esp_nn_add_elementwise_s8(...)`
- `fully_connected.cc`: the `#if ESP_NN` just before `esp_nn_fully_connected_s8(...)`

### 3e. `softmax.cc` — route to ansi (precautionary)
After its `#include ".../esp_nn.h"`:
```c
#undef esp_nn_softmax_s8
#define esp_nn_softmax_s8 esp_nn_softmax_s8_ansi
#undef esp_nn_get_softmax_scratch_size
#define esp_nn_get_softmax_scratch_size esp_nn_get_softmax_scratch_size_ansi
#undef esp_nn_set_softmax_scratch_buf
#define esp_nn_set_softmax_scratch_buf esp_nn_set_softmax_scratch_buf_ansi
```
(Softmax is monotonic so it can't flip the decision; this is just to stay on the
known-good path.)

**Leave `conv.cc` alone** — esp-nn's optimized conv (1×1 and 3×3) is bit-exact
correct on this toolchain and is the bulk of the speedup. Don't patch it.

---

## 4. Verify (every time)

Build + flash `MLB_Muter_s3`, open the serial monitor (115200). `setup()` runs
the 10 embedded `Test_Images` and prints `Predictions:` + `Timing:`.

Expected — must match the reference build:
| Images | Expected winner |
|--------|-----------------|
| 1–5    | `program`       |
| 6–10   | `commercial`    |

(Image 5 is borderline ~0.57 program; image 10 ~0.97 commercial — that's normal.)
`classification` should be **~489 ms**. If it's:
- **~2.4 s** → ESP-NN didn't engage (check `CONFIG_NN_OPTIMIZED` + the `-I` flags + `=1`).
- **biased toward one class** → a broken kernel slipped back onto the optimized
  path; re-check 3c/3d. To sanity-check which esp-nn kernels are (mis)computed on
  the current toolchain, see the `espNn*SelfTest()` functions in git history
  (commit before `0342932`) — they compare each optimized kernel vs its `_ansi`.
- **boot loop / `CORRUPT HEAP` / `LoadProhibited`** → see notes below.

---

## Gotchas / things we already learned the hard way
- **Internal-SRAM arena crashes.** `-DEI_CLASSIFIER_ALLOCATION_STATIC` (arena in
  internal SRAM) is ~30 ms faster in theory but the 223 KB arena is too tight for
  esp-nn scratch → `LoadProhibited`. Keep the arena in PSRAM (just don't define
  that flag; arena size 524288).
- **EON model + ESP-NN = heap corruption.** Must use the non-EON TensorFlow Lite
  deployment (Step 0).
- **Don't reduce the arena** below ~200 KB or AllocateTensors fails (-3 /
  "AllocateTensors() failed").
- If you ever want to bail entirely: set `-DEI_CLASSIFIER_TFLITE_ENABLE_ESP_NN=0`
  → all reference kernels, correct, ~2.4 s. That's effectively `main`.

Full root-cause write-up is in the agent memory file
`esp-nn-simd-investigation.md`.
