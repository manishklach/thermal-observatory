# Review Notes

## Summary

The original draft is directionally strong and ambitious. It covers most of the right Linux thermal surfaces for x86, arm64, NVIDIA, and AMD. The main problem was not lack of ideas, but lack of separation between:

- stable vs experimental interfaces
- collection vs presentation
- authoritative metrics vs derived or mislabeled metrics

## Concrete Issues In The Draft

1. `Makefile.userspace` expected a `src/` and `headers/` layout that did not exist yet.
2. `thermal_main.c` mostly orchestrated printing helpers rather than collecting a shared snapshot model.
3. Several functions were named as if they were returning “power” while actually reading cumulative energy counters.
4. The kernel module draft included code patterns that are not ready to compile cleanly in-tree, for example loop constructs and thermal iteration assumptions that need tightening against real kernel APIs.
5. The public header had grown into a very large mixed contract, combining stable API ideas with draft-only internals.

## What I Changed

- introduced a new repo structure around a normalized snapshot API
- separated formatters from collectors
- isolated the kernel path as explicitly experimental
- kept Linux sysfs, NVML, and ROCm SMI as the preferred userspace interfaces
- added top-level README plus design and architecture docs

## What Still Needs Real Linux Validation

- compiling and running on x86_64 Linux
- compiling and running on arm64 Linux
- validating NVML symbol coverage across driver versions
- validating ROCm SMI symbol coverage across ROCm releases
- deciding whether the kernel module should become a real char-device/ioctl interface or stay as a research-only branch

