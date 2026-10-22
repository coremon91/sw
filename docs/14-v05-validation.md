# SW 0.5 validation (2026-10-23)

- Windows Release build and executable version 0.5.0.0 verified.
- CTest: 7/7 passed (core, media fixture, media, routing, CUDA transfer, D3D11 transfer, CG Key/Fill).
- Synthetic HD engine, live cadence switching without restart:
  - interval 1: 120 ticks / 120 monitor frames; 59.9001 fps
  - interval 2: 120 ticks / 60 monitor frames; 29.7437 fps
  - interval 4: 120 ticks / 30 monitor frames; 14.9747 fps
  - interval 1 again: 120 ticks / 120 monitor frames; 59.9621 fps
- Packaged GUI layout: fixed_footer_stable and variable_footer_wrap_verified passed.
- Existing 0.3 and 0.4 executable SHA256 hashes unchanged.
- Live SDI output was not restarted or used for these tests. Physical output smoothness and end-to-end latency remain unverified for 0.5.
