# Project workflow

- Preserve previous version distributions when preparing a new version.
- The user requires all future software updates to be committed and uploaded to the GitHub repository coremon91/sw after validation. Report the actual commit/branch and any upload failure; do not claim publication before verifying it.
- Do not commit local dependencies, vendor SDKs, runtime logs, captured media, or credentials.
- Validate every upgrade in all four live window states (foreground, partly visible/inactive, fully occluded, minimized) before promoting it as verified. Preserve routing/CG settings and record measured intervals/results; label blocked or unperformed checks explicitly.
