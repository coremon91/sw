"use strict";

const previewFrame = document.getElementById("previewFrame");
const previewViewport = document.getElementById("previewViewport");
const statusElement = document.getElementById("status");
const serviceStatusElement = document.getElementById("serviceStatus");
const styleFields = [
  "province",
  "intervalSeconds",
  "fontSize",
  "right",
  "bottom",
  "width"
];
const numericFields = new Set([
  "intervalSeconds",
  "fontSize",
  "right",
  "bottom",
  "width"
]);
const weather = nodecg.Replicant("weatherPreview");
const serviceStatus = nodecg.Replicant("weatherServiceStatus");
const connection = nodecg.Replicant("casparConnection");
let currentItems = [];
let manualWeatherDirty = false;
const dirtyStyleFields = new Set();

function resizePreview() {
  previewFrame.style.transform =
    `scale(${previewViewport.clientWidth / 1920})`;
}
new ResizeObserver(resizePreview).observe(previewViewport);
resizePreview();

function updateOutputs() {
  const units = {
    intervalSeconds: "초",
    fontSize: "px",
    right: "px",
    bottom: "px",
    width: "px"
  };
  Object.keys(units).forEach((id) => {
    document.getElementById(`${id}Output`).textContent =
      `${document.getElementById(id).value}${units[id]}`;
  });
}

weather.on("change", (value) => {
  if (!value) return;
  value = SWBroadcastLayout.weather(value);
  if (!value.province || ["전라남도", "광주광역시"].includes(value.province)) value.province = "전남광주특별시";
  currentItems = Array.isArray(value.items)
    ? JSON.parse(JSON.stringify(value.items))
    : [];
  styleFields.forEach((id) => {
    if (value[id] != null && !dirtyStyleFields.has(id)) document.getElementById(id).value = value[id];
  });
  const first = Array.isArray(value.items) ? value.items[0] : null;
  if (first && !manualWeatherDirty) {
    document.getElementById("city").value = first.city || "";
    document.getElementById("temp").value = first.temp || "";
    document.getElementById("condition").value =
      first.condition || "맑음";
  }
  updateOutputs();
});

serviceStatus.on("change", (value) => {
  if (!value) return;
  serviceStatusElement.textContent = `● ${value.message}`;
  serviceStatusElement.className = value.ok ? "ok" : "bad";
});

connection.on("change", (value) => {
  if (!value) return;
  statusElement.textContent = `● ${value.message}`;
  statusElement.className = value.connected ? "ok" : "bad";
});

function collect() {
  const value = { layoutVersion: SWBroadcastLayout.version };
  styleFields.forEach((id) => {
    const raw = document.getElementById(id).value;
    value[id] = numericFields.has(id) ? Number(raw) : raw;
  });
  value.items = manualWeatherDirty
    ? [
        {
          city: document.getElementById("city").value.trim(),
          temp: document.getElementById("temp").value.trim(),
          condition: document.getElementById("condition").value
        }
      ]
    : currentItems;
  value.lastUpdated = "";
  return value;
}

async function apply() {
  await nodecg.sendMessage("weather:update", collect());
  dirtyStyleFields.clear(); manualWeatherDirty = false;
  statusElement.textContent =
    "● 프리뷰 적용 완료 · 현재 송출 화면은 변경되지 않습니다";
  statusElement.className = "ok";
}

async function refreshWeather() {
  try {
    const province = document.getElementById("province").value;
    await nodecg.sendMessage("weather:update", { province });
    await nodecg.sendMessage("weather:refresh", { province });
  } catch (error) {
    serviceStatusElement.textContent = `● ${error.message || String(error)}`;
    serviceStatusElement.className = "bad";
  }
}

async function run(action) {
  try {
    await apply();
    await nodecg.sendMessage(`weather:${action}`);
    const messages = {
      show: "● 날씨 CG 송출 TAKE 완료",
      programUpdate: "● 현재 날씨 송출에 프리뷰 내용 반영 완료",
      hide: "● 날씨 CG 송출 OUT 완료"
    };
    statusElement.textContent = messages[action] || "● 처리 완료";
    statusElement.className = "ok";
  } catch (error) {
    statusElement.textContent = error.message || String(error);
    statusElement.className = "bad";
  }
}

document.querySelectorAll('input[type="range"]').forEach((input) =>
  input.addEventListener("input", updateOutputs)
);
styleFields.forEach(id=>{
  const input=document.getElementById(id);
  input.addEventListener(input.tagName==="SELECT"?"change":"input",()=>dirtyStyleFields.add(id));
});
["city", "temp", "condition"].forEach((id) => {
  const element = document.getElementById(id);
  element.addEventListener(
    element.tagName === "SELECT" ? "change" : "input",
    () => {
      manualWeatherDirty = true;
    }
  );
});
document.getElementById("refreshPreview").addEventListener("click", () => {
  previewFrame.src =
    `/bundles/my-cg/graphics/weather.html?preview=1&v=${Date.now()}`;
});
document.getElementById("refreshWeather").addEventListener("click", refreshWeather);
document.getElementById("apply").addEventListener("click", () =>
  apply().catch((error) => {
    statusElement.textContent = error.message || String(error);
    statusElement.className = "bad";
  })
);
document.getElementById("take").addEventListener("click", () => run("show"));
document.getElementById("updateProgram").addEventListener("click", () =>
  run("programUpdate")
);
document.getElementById("out").addEventListener("click", () => run("hide"));
