# SW 0.7.3 — background timer policy
Windows 11 may ignore timeBeginPeriod requests when an app is fully occluded or minimized. The engine now explicitly disables PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION, without changing execution-speed QoS. Logs record policy success/error and timeBeginPeriod result. Unsupported/failed policy application remains visible in logs; the engine retains its previous wait path.
Reference: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocessinformation

## Validation, 2026-10-23
Release build, 7 CTests, and --self-test-layout passed. Existing distributions were retained; package: dist/SW-0.7.3.
Live UHD2160p59.94, D3D11/CUDA transfer, background IN1, Fill IN2, Key IN4, both CG buses on, output muted, preview interval 1.
Each phase used a 30-second log selection window beginning 3 seconds after the state change. Actual counter intervals were about 29.23 seconds.

| State | Deadline misses added | Render snapshot mean ms | PGM/PVW drops added |
|---|---:|---:|---:|
| Foreground | 0 | 5.84 | 0/0 |
| Partly visible, inactive | 0 | 6.03 | 0/0 |
| Fully occluded | 0 | 6.17 | 0/0 |
| Minimized | 0 | 5.92 | 0/0 |

All selected samples reported successful timer policy and timer resolution requests. Audio underrun/overflow counters did not increase. Initial startup had one PVW drop outside these measurement windows.
The 0.7.2 baseline had 0, 0, 407, 344 deadline misses respectively under the same routing and preview cadence. This supports timer policy as a major contributor to the background scheduling fault. Render means are approximately one-second snapshots, not exhaustive per-frame statistics. External SDI scrolling was not recorded/analyzed, so this is timing validation rather than proof of complete visual symptom removal.

## SW 0.7.4 — multimedia worker and cancellable high-resolution wait
Retains 0.7.3 timer policy. The worker registers with MMCSS Playback and waits on a high-resolution waitable timer plus a manual-reset stop event. The absolute 59.94 tick schedule remains unchanged. Stop signals the event before joining; restart clears it. Failed timer creation/arming/wait falls back to the existing condition-variable path. Timer/MMCSS application and errors are recorded. Handles and MMCSS registration are released on worker exit.

Release build, all 7 CTests, --self-test-layout and --engine (start, live commands, transition, stop) passed. Package: dist/SW-0.7.4. No previous distribution overwritten.

| State | Deadline misses added | Render snapshot mean ms | PGM/PVW drops added |
|---|---:|---:|---:|
| Foreground | 0 | 6.32 | 0/0 |
| Partly visible, inactive | 0 | 6.30 | 0/0 |
| Fully occluded | 0 | 6.40 | 0/0 |
| Minimized | 0 | 6.30 | 0/0 |

Same live configuration and selection method as 0.7.3; approximately 29.23 seconds per counter interval. MMCSS and high-resolution wait successfully applied, policy successful, no audio underrun/overflow increases. These short tests establish no observed regression; they do not demonstrate additional visual improvement over 0.7.3 or long-duration broadcast reliability.
