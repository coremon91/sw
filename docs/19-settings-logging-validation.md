# SW 0.7.2 validation

- Explicit routing/settings save and load added using a versioned JSON object in QSettings. Saved settings restore on startup; loading is disabled while a session is running or starting.
- Missing endpoints remain identified as missing instead of being silently replaced. Invalid preset structure is rejected before applying changes.
- Preview refresh selection and measured preview rate are located in Settings. Existing switcher preview-rate controls were removed.
- Session logging defaults to enabled; JSONL snapshots are written approximately once per second on the GUI thread, outside the video processing loop. Start/end and recording-disabled records are included. Files split at approximately 20 MiB, without deleting old logs.
- Logs contain operational counters, not captured video/audio. External player process internals are not logged. Short events between samples and GUI stalls can be missed.

## Validation on 2026-10-23

Packaged executable built and ran successfully with `--self-test-layout`:

```json
{"fixed_footer_stable":true,"variable_footer_wrap_verified":true}
{"embedded_guide_loaded":true}
{"routing_in_settings":true}
{"settings_round_trip":true}
{"session_log_round_trip":true}
```

Settings were serialized to a temporary QSettings INI file and read back, then applied to the stopped UI. An invalid source mode was rejected without changing current settings. The logger wrote start/sample/end JSONL records into a temporary directory and the records were parsed back. Tests did not replace the user's saved preset or start physical SDI outputs.

Actual long-running SDI playback and disk-full behavior have not been exercised. Prior distributions remain in their original folders; the new package is `dist/SW-0.7.2`.
