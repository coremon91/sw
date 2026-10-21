"use strict";

const previewFrame = document.getElementById("previewFrame");
const previewViewport = document.getElementById("previewViewport");
const dragHandle = document.getElementById("dragHandle");
const statusElement = document.getElementById("status");
const fields = ["image", "x", "y", "width", "height", "opacity"];
const numericFields = new Set(["x", "y", "width", "height", "opacity"]);
const logo = nodecg.Replicant("logoPreview");
const connection = nodecg.Replicant("casparConnection");
let latestLogoValue;
let dashboardDragState;

function resizePreview() {
  const scale = previewViewport.clientWidth / 1920;
  previewFrame.style.transform = `scale(${scale})`;
  updateDragHandle();
}

new ResizeObserver(resizePreview).observe(previewViewport);
resizePreview();

function fieldNumber(id, fallback) {
  const raw = document.getElementById(id).value;
  const value = Number(raw);
  return raw !== "" && Number.isFinite(value) ? value : fallback;
}

logo.on("change", (value) => {
  if (!value) return;
  value = SWBroadcastLayout.logo(value);
  latestLogoValue = value;
  fields.forEach((id) => {
    const element = document.getElementById(id);
    if (value[id] != null) element.value = value[id];
  });
  updateDragHandle();
  setTimeout(updateDragHandle, 0);
});

function updateDragHandle() {
  const scale = previewViewport.clientWidth / 1920;
  const x = fieldNumber("x", Number(latestLogoValue?.x) || 0);
  const y = fieldNumber("y", Number(latestLogoValue?.y) || 0);
  const width = fieldNumber(
    "width",
    Number(latestLogoValue?.width) || 180
  );
  const rawHeight = fieldNumber(
    "height",
    Number(latestLogoValue?.height) || width
  );
  const height = rawHeight > 0 ? rawHeight : width;
  dragHandle.style.left = `${x * scale}px`;
  dragHandle.style.top = `${y * scale}px`;
  dragHandle.style.width = `${width * scale}px`;
  dragHandle.style.height = `${height * scale}px`;
}

connection.on("change", (value) => {
  if (!value) return;
  statusElement.textContent = `● ${value.message}`;
  statusElement.className = value.connected ? "ok" : "bad";
});

function collect() {
  const value = { layoutVersion: SWBroadcastLayout.version };
  fields.forEach((id) => {
    const raw = document.getElementById(id).value;
    if (!numericFields.has(id)) {
      value[id] = raw.trim();
    } else if (id === "height" && Number(raw) <= 0) {
      value[id] = fieldNumber("width", 180);
    } else {
      value[id] = Number(raw);
    }
  });
  return value;
}

async function pushPreview(showStatus = true) {
  await nodecg.sendMessage("logo:update", collect());
  if (showStatus) {
    statusElement.textContent =
      "● 프리뷰 실시간 반영 완료 · 현재 송출 화면은 변경되지 않습니다";
    statusElement.className = "ok";
  }
}

async function run(action) {
  try {
    clearTimeout(liveUpdateTimer);
    await pushPreview(false);
    await nodecg.sendMessage(`logo:${action}`);
    const messages = {
      show: "● 로고 송출 TAKE 완료",
      programUpdate: "● 현재 로고 송출에 프리뷰 내용 반영 완료",
      hide: "● 로고 송출 OUT 완료"
    };
    statusElement.textContent = messages[action] || "● 처리 완료";
    statusElement.className = "ok";
  } catch (error) {
    statusElement.textContent = error.message || String(error);
    statusElement.className = "bad";
  }
}

let liveUpdateTimer;
function scheduleLiveUpdate(delay = 70) {
  clearTimeout(liveUpdateTimer);
  liveUpdateTimer = setTimeout(() => {
    pushPreview().catch((error) => {
      statusElement.textContent = error.message || String(error);
      statusElement.className = "bad";
    });
  }, delay);
}

document.querySelectorAll("input").forEach((input) => {
  input.addEventListener("input", () => {
    scheduleLiveUpdate(input.id === "image" ? 220 : 70);
  });
});
previewFrame.addEventListener("load", updateDragHandle);

dragHandle.addEventListener("pointerdown", (event) => {
  event.preventDefault();
  dragHandle.setPointerCapture(event.pointerId);
  dragHandle.classList.add("dragging");
  dashboardDragState = {
    pointerId: event.pointerId,
    clientX: event.clientX,
    clientY: event.clientY,
    x: Number(document.getElementById("x").value) || 0,
    y: Number(document.getElementById("y").value) || 0
  };
});

dragHandle.addEventListener("pointermove", (event) => {
  if (
    !dashboardDragState ||
    dashboardDragState.pointerId !== event.pointerId
  ) {
    return;
  }
  const scale = previewViewport.clientWidth / 1920;
  const width = fieldNumber("width", 180);
  const height = fieldNumber("height", width);
  const x = Math.round(
    Math.min(
      1920 - width,
      Math.max(
        0,
        dashboardDragState.x +
          (event.clientX - dashboardDragState.clientX) / scale
      )
    )
  );
  const y = Math.round(
    Math.min(
      1080 - height,
      Math.max(
        0,
        dashboardDragState.y +
          (event.clientY - dashboardDragState.clientY) / scale
      )
    )
  );
  document.getElementById("x").value = x;
  document.getElementById("y").value = y;
  dragHandle.style.left = `${x * scale}px`;
  dragHandle.style.top = `${y * scale}px`;
  scheduleLiveUpdate(35);
});

function finishDashboardDrag(event) {
  if (
    !dashboardDragState ||
    dashboardDragState.pointerId !== event.pointerId
  ) {
    return;
  }
  dashboardDragState = null;
  dragHandle.classList.remove("dragging");
  clearTimeout(liveUpdateTimer);
  pushPreview().catch((error) => {
    statusElement.textContent = error.message || String(error);
    statusElement.className = "bad";
  });
}

dragHandle.addEventListener("pointerup", finishDashboardDrag);
dragHandle.addEventListener("pointercancel", finishDashboardDrag);
document.getElementById("refreshPreview").addEventListener("click", () => {
  previewFrame.src =
    `/bundles/my-cg/graphics/logo.html?preview=1&v=${Date.now()}`;
});
document.getElementById("take").addEventListener("click", () => run("show"));
document.getElementById("updateProgram").addEventListener("click", () =>
  run("programUpdate")
);
document.getElementById("out").addEventListener("click", () => run("hide"));
