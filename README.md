# mtkcam - MediaTek Camera kernel module repository

## WHAT IT DOES?

This repository places out-of-tree MediaTek camera kernel modules.
The folder structure is listed below:

```bash
mtkcam/
├── include/
├── camsys/
│   ├── isp7_1/
│   ├── isp7s/
│   ├── isp7sp/
│   ├── remoteproc/
│   ├── rpmsg/
│   └── common/
├── imgsensor/
│   ├── src-v4l2/
│   └── inc/
├── mtk-aie/
├── mtk-dpe/
├── mtk-hcp/
├── mtk-ipesys-me/
└── scpsys/
```

### include

The `include` subdirectory places the public kernel header files that
are used by kernel modules in the `mtkcam` directory.

### camsys

The `camsys` subdirectory contains ISP7.x kernel module implementations
from ISP7.1 to ISP7sp.

We opt to support Linux kernel version within the same ISP kernel module
 implementation by using the macro
`LINUX_VERSION_CODE` defined in `linux/version.h`.

### imgsensor
_TODO: need documentation contribution..._

### mtk-aie
_TODO: need documentation contribution..._

### mtk-dpe
_TODO: need documentation contribution..._

### mtk-hcp
_TODO: need documentation contribution..._

### mtk-ipesys-me
_TODO: need documentation contribution..._

### scpsys
_TODO: need documentation contribution..._

## HOW IT WAS BUILT?

[Kleaf](https://android.googlesource.com/kernel/build/+/refs/heads/master/kleaf/README.md), which is a new build flow, only supports kernel version from 6.1.
And it has been launched in the mgk_64_k61 at first.

### MGK (Mediatek Generic Kernel) build command

Same as before:

```bash
source build/envsetup.sh
export OUT_DIR=out_krn
lunch {project name}-{build variant}
make -j<number of simultaneous jobs> krn_images
```

#### Example - MGK build

{project name} is `krn_mgk_64_k61`, {build variant} is `userdebug` and
\<number of simultaneous jobs\> is `16`:

```bash
source build/envsetup.sh
export OUT_DIR=out_krn
lunch krn_mgk_64_k61-userdebug
make -j16 krn_images
```

### Standalone build command

The fundamental command is

```bash
 cd kernel
 ./tools/bazel build <options> <build setting> //{package name}:{target name}
```

#### Example - camsys standalone build

- \<options\> are `--verbose_failures --sandbox_debug`
- \<build setting\> is `--//build/bazel_mgk_rules:kernel_version=6.1`
- {pacakge name} is `vendor/mediatek/kernel_modules/mtkcam/camsys`
- {target name} is `camsys.6.1.userdebug`

```bash
cd kernel
./tools/bazel build --verbose_failures --sandbox_debug --//build/bazel_mgk_rules:kernel_version=6.1 //vendor/mediatek/kernel_modules/mtkcam/camsys:camsys.6.1.userdebug
```

The mtkcam kernel modules should be put into the `/vendor/lib/modules` folder.

## HOW TO USE IT?

mtkcam kernel modules provide Linux kernel user-space API in the
`device/mediatek/common/kernel-headers` folder.