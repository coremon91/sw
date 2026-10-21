"use strict";

// Loaded by my-cg's extension. Explicit mode selection keeps the original
// CasparCG transport available without requiring it for software compositing.
module.exports = function internalPlayout(nodecg) {
  const internal = process.env.SW_INTERNAL_CG === "1" ||
    nodecg.bundleConfig.playoutMode === "internal";
  nodecg.Replicant("swPlayout", {
    persistent: false,
    defaultValue: { mode: internal ? "internal" : "casparcg" }
  });
  return internal;
};
