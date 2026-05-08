# Design

## Goals

- Normalize thermal telemetry across CPU and GPU vendors.
- Prefer authoritative userspace interfaces over raw register scraping.
- Make privilege boundaries explicit.
- Keep vendor- or kernel-specific code modular.

## Non-goals

- Shipping a full production daemon in the first revision.
- Replacing vendor tooling such as `nvidia-smi` or `rocm-smi`.
- Pretending every metric exists on every platform.

## Design Rules

1. One snapshot model.
2. Each collector only fills fields it can prove.
3. Missing data is acceptable; fake normalization is not.
4. Rendering is separate from collection.
5. Experimental kernel work stays isolated from the default build/run path.

## What Needed Cleanup From The Draft

The original draft had strong subsystem coverage, but several issues made it hard to maintain:

- collection and pretty-printing were tightly coupled
- the public header mixed API, vendor detail, and kernel-module assumptions
- the userspace `Makefile` referenced a directory structure that did not yet exist
- the kernel module mixed good ideas with code that is not kernel-buildable as written
- some metrics were described as instantaneous power while the code was actually reading cumulative energy counters

This repo fixes that by making the snapshot schema the center of the design.

## Data Model

The normalized model groups data into:

- host metadata
- CPU packages and cores
- ARM clusters and zones
- NVIDIA GPUs
- AMD GPUs
- generic `hwmon` and thermal zones

Each collector can set capability bits to explain what it actually observed.

## Collection Order

1. Generic Linux platform discovery
2. CPU-specific collectors
3. GPU-specific collectors
4. Optional experimental kernel collector
5. Formatting into text or JSON

## Safety Model

Default operation is read-only userspace inspection.

The kernel module is marked experimental because:

- direct MSR and PCI access are platform-sensitive
- some register families vary by CPU generation
- in-kernel IPMI and deeper PCI probing need much stricter hardening before production use

