# 0.7.1 routing settings update

- Move the existing mode/format, IN 1-4, PGM/PVW output, device refresh, and session start/stop controls into Settings > I/O routing.
- Keep the guide in a separate Settings > Usage guide subtab; provide an I/O settings shortcut from the switcher.
- Preserve the original 0.7 distribution in dist/SW. New package: dist/SW-0.7.1.
- Release build succeeded; executable version 0.7.1.0 verified.
- Added routing ancestry assertions to the existing GUI layout check, but the packaged executable could not be started: Windows application control policy blocked execution. GUI runtime validation remains pending. No bypass was attempted.
