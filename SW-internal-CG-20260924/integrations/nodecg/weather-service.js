"use strict";

const fs = require("node:fs");
const path = require("node:path");

const LEGACY_WEATHER_SOURCE =
  "D:\\broadcastprojects\\nodecg-election\\nodecg\\bundles\\" +
  "my-election-bundle\\extension\\weather.js";
const OBSERVATION_URL =
  "https://apis.data.go.kr/1360000/VilageFcstInfoService_2.0/getUltraSrtNcst";
const FORECAST_URL =
  "https://apis.data.go.kr/1360000/VilageFcstInfoService_2.0/getUltraSrtFcst";

const CITY_MAP = {
  전남광주특별시: [
    { city: "광주", nx: 58, ny: 74 },
    { city: "목포", nx: 50, ny: 67 },
    { city: "여수", nx: 73, ny: 66 },
    { city: "순천", nx: 70, ny: 69 },
    { city: "나주", nx: 58, ny: 72 },
    { city: "광양", nx: 74, ny: 70 }
  ]
};
const normalizeProvince = value => !value || ["전라남도", "광주광역시"].includes(value)
  ? "전남광주특별시" : String(value);

module.exports = function registerWeather(nodecg, playout) {
  const weatherConfig = nodecg.bundleConfig.weather || {};
  const configuredServiceKey = String(
    weatherConfig.serviceKey || process.env.KMA_SERVICE_KEY || ""
  ).trim();
  let serviceKeySource = configuredServiceKey ? "config" : "";
  let serviceKey = configuredServiceKey;
  const keyFile = path.resolve(__dirname, "../../../cfg/my-cg-weather-key.json");
  try {
    const saved = JSON.parse(fs.readFileSync(keyFile, "utf8"));
    if (typeof saved.serviceKey === "string" && saved.serviceKey.trim()) {
      serviceKey = saved.serviceKey.trim(); serviceKeySource = "dashboard";
    }
  } catch (error) {
    if (error.code !== "ENOENT") nodecg.log.warn("Saved weather key could not be loaded.");
  }

  if (!serviceKey) {
    try {
      const legacySource = fs.readFileSync(LEGACY_WEATHER_SOURCE, "utf8");
      const match = legacySource.match(
        /const\s+serviceKey\s*=\s*['"]([^'"]+)['"]/
      );
      if (match?.[1]) {
        serviceKey = match[1].trim();
        serviceKeySource = "legacy";
      }
    } catch (error) {
      nodecg.log.debug(
        "Legacy weather API config was not loaded: %s",
        error.message
      );
    }
  }

  const defaultValue = {
    visible: false,
    province: "전남광주특별시",
    items: [{ city: "광주", temp: "25℃", condition: "맑음" }],
    intervalSeconds: 5,
    fontSize: 38,
    right: 60,
    bottom: 60,
    width: 350,
    lastUpdated: ""
  };

  const program = nodecg.Replicant("weather", { defaultValue });
  const preview = nodecg.Replicant("weatherPreview", {
    defaultValue: JSON.parse(JSON.stringify(program.value))
  });
  // Migrate the region label without replacing weather data or changing output visibility.
  for (const rep of [program, preview]) {
    if (rep.value.province !== normalizeProvince(rep.value.province)) {
      rep.value = { ...rep.value, province: normalizeProvince(rep.value.province) };
    }
  }
  const status = nodecg.Replicant("weatherServiceStatus", {
    persistent: false,
    defaultValue: {
      loading: false,
      ok: true,
      message: serviceKey
        ? serviceKeySource === "legacy"
          ? "D 드라이브 기존 날씨 API 설정 적용됨"
          : "날씨 API 자동 조회 준비됨"
        : "수동 입력 모드 · API 키 미설정"
    }
  });
  const target = `${playout.caspar.channel}-${playout.caspar.layer + 4}`;
  const keyStatus = () => ({configured:Boolean(serviceKey), source:serviceKeySource});
  nodecg.listenFor("weather:keyStatus", (_data, ack) => {
    if (ack && !ack.handled) ack(null, keyStatus());
  });
  nodecg.listenFor("weather:saveKey", (data, ack) => {
    try {
      let key = String(data?.serviceKey || "").trim();
      if (key.includes("%")) key = decodeURIComponent(key);
      if (key.length < 8 || key.length > 2048 || /\s/.test(key)) throw Error("API 인증키 형식을 확인하세요.");
      const temporary = keyFile + ".tmp";
      fs.mkdirSync(path.dirname(keyFile), {recursive:true});
      fs.writeFileSync(temporary, JSON.stringify({serviceKey:key}), {encoding:"utf8",mode:0o600});
      fs.renameSync(temporary, keyFile);
      serviceKey = key; serviceKeySource = "dashboard";
      status.value = {loading:false,ok:true,message:"API 키 저장 완료 · 새로고침으로 조회를 확인하세요."};
      if (ack && !ack.handled) ack(null, keyStatus());
    } catch (_error) {
      if (ack && !ack.handled) ack(new Error("API 키를 저장하지 못했습니다. 인증키 형식과 설정 폴더 쓰기 권한을 확인하세요."));
    }
  });

  function copy(value) {
    return JSON.parse(JSON.stringify(value));
  }

  function updatePreview(data) {
    preview.value = {
      ...preview.value,
      ...data,
      province: normalizeProvince(data.province || preview.value.province),
      visible: false
    };
  }

  function getBaseDateTime(offsetMinutes, halfHourly) {
    const date = new Date(Date.now() - offsetMinutes * 60 * 1000);
    const year = String(date.getFullYear());
    const month = String(date.getMonth() + 1).padStart(2, "0");
    const day = String(date.getDate()).padStart(2, "0");
    const hour = String(date.getHours()).padStart(2, "0");
    const minute =
      halfHourly && date.getMinutes() >= 30 ? "30" : "00";
    return {
      baseDate: `${year}${month}${day}`,
      baseTime: `${hour}${minute}`
    };
  }

  function describeCondition(precipitation, sky) {
    const rainCode = Number(precipitation || 0);
    if (rainCode === 1) return "비";
    if (rainCode === 2) return "비/눈";
    if (rainCode === 3) return "눈";
    if (rainCode === 4) return "소나기";
    if (Number(sky) === 3) return "구름 많음";
    if (Number(sky) === 4) return "흐림";
    return "맑음";
  }

  async function fetchCityWeather(city) {
    if (!serviceKey) {
      throw new Error(
        "날씨 설정에서 기상청 API 인증키를 저장해야 자동 조회할 수 있습니다."
      );
    }

    const timeoutMs = Number(weatherConfig.requestTimeoutMs) || 8000;

    async function request(endpoint, base) {
      const query = new URLSearchParams({
        serviceKey,
        pageNo: "1",
        numOfRows: "1000",
        dataType: "JSON",
        base_date: base.baseDate,
        base_time: base.baseTime,
        nx: String(city.nx),
        ny: String(city.ny)
      });
      const response = await fetch(`${endpoint}?${query}`, {
        signal: AbortSignal.timeout(timeoutMs)
      });
      if (!response.ok) {
        throw new Error(`기상청 API HTTP ${response.status}`);
      }
      const text = await response.text();
      if (text.trimStart().startsWith("<")) {
        throw new Error("기상청 API가 XML 오류 응답을 반환했습니다.");
      }
      const json = JSON.parse(text);
      const header = json?.response?.header;
      if (header?.resultCode && header.resultCode !== "00") {
        throw new Error(header.resultMsg || `API 오류 ${header.resultCode}`);
      }
      return json?.response?.body?.items?.item || [];
    }

    const responses = await Promise.allSettled([
      request(OBSERVATION_URL, getBaseDateTime(40, false)),
      request(FORECAST_URL, getBaseDateTime(45, true))
    ]);
    const failed = responses.find(result => result.status === "rejected");
    if (failed) throw failed.reason;
    const [observations, items] = responses.map(result => result.value);
    const forecastKeys = [
      ...new Set(
        items
          .filter((item) => item.fcstDate && item.fcstTime)
          .map((item) => `${item.fcstDate}${item.fcstTime}`)
      )
    ].sort();
    if (forecastKeys.length === 0) {
      throw new Error(`${city.city} 예보 데이터가 없습니다.`);
    }

    const now = new Date();
    const nowKey = [
      now.getFullYear(),
      String(now.getMonth() + 1).padStart(2, "0"),
      String(now.getDate()).padStart(2, "0"),
      String(now.getHours()).padStart(2, "0"),
      String(now.getMinutes()).padStart(2, "0")
    ].join("");
    const selectedKey =
      forecastKeys.find((key) => key >= nowKey) ||
      forecastKeys[forecastKeys.length - 1];
    const selected = items.filter(
      (item) => `${item.fcstDate}${item.fcstTime}` === selectedKey
    );
    const valueOf = (category) =>
      selected.find((item) => item.category === category)?.fcstValue;
    const observationValueOf = (category) =>
      observations.find((item) => item.category === category)?.obsrValue;
    const temperature = observationValueOf("T1H");

    return {
      city: city.city,
      temp: temperature == null ? "N/A" : `${temperature}℃`,
      condition: describeCondition(
        observationValueOf("PTY"),
        valueOf("SKY")
      )
    };
  }

  nodecg.listenFor("weather:update", (data, ack) => {
    updatePreview(data || {});
    if (ack && !ack.handled) ack(null, preview.value);
  });

  let refreshing = false;
  async function refreshWeather(province, automatic = false) {
    const cities = CITY_MAP[province];
    if (!cities) throw new Error(`지원하지 않는 지역입니다: ${province}`);
    if (refreshing) return {ok:false,busy:true};
    refreshing = true;

    status.value = {
      loading: true,
      ok: true,
      message: `${province} 날씨 조회 중…`
    };
    try {
      const results = await Promise.allSettled(cities.map(fetchCityWeather));
      const failure = results.find(result => result.status === "rejected");
      if (failure) throw failure.reason;
      const items = results.map(result => result.value);
      const lastUpdated = new Intl.DateTimeFormat("ko-KR", {
        hour: "2-digit",
        minute: "2-digit",
        second: "2-digit"
      }).format(new Date());
      updatePreview({ province, items, lastUpdated });
      if (automatic) program.value = { ...program.value, province, items:copy(items), lastUpdated };
      status.value = {
        loading: false,
        ok: true,
        message: `${items.length}개 지역 갱신 · ${lastUpdated} · 5초 자동 조회`,
        lastSuccessAt: new Date().toISOString(),
        autoRefreshSeconds: 5
      };
      return { ok:true, count:items.length };
    } catch (error) {
      status.value = {
        loading: false,
        ok: false,
        message: `${error.message} · 마지막 정상 값 유지 · 5초 간격 재시도`,
        autoRefreshSeconds: 5
      };
      nodecg.log.warn("Weather refresh failed: %s", error.message);
      throw error;
    } finally {
      refreshing = false;
    }
  }
  nodecg.listenFor("weather:refresh", async (data, ack) => {
    try {
      const result = await refreshWeather(normalizeProvince(data?.province || preview.value.province));
      if (ack && !ack.handled) ack(null,result);
    } catch (error) { if (ack && !ack.handled) ack(error); }
  });
  const autoRefreshTimer = setInterval(async () => {
    if (!serviceKey || refreshing) return;
    try { await refreshWeather("전남광주특별시",true); } catch { /* Keep the last good data; retry on the next tick. */ }
  },5000);
  autoRefreshTimer.unref?.();

  nodecg.listenFor("weather:show", async (_data, ack) => {
    try {
      program.value = { ...copy(preview.value), visible: true };
      const separator = playout.graphicUrl.includes("?") ? "&" : "?";
      const url = `${playout.graphicUrl}${separator}v=${Date.now()}`;
      await playout.send(
        `PLAY ${target} [HTML] "${url.replace(/"/g, '\\"')}"`
      );
      if (ack && !ack.handled) ack(null, { ok: true });
    } catch (error) {
      if (ack && !ack.handled) ack(error);
    }
  });

  nodecg.listenFor("weather:programUpdate", (_data, ack) => {
    const wasVisible = Boolean(program.value.visible);
    program.value = { ...copy(preview.value), visible: wasVisible };
    if (ack && !ack.handled) ack(null, { ok: true });
  });

  nodecg.listenFor("weather:hide", async (_data, ack) => {
    try {
      program.value = { ...program.value, visible: false };
      setTimeout(() => {
        playout.send(`STOP ${target}`).catch((error) =>
          nodecg.log.warn("CasparCG weather STOP failed: %s", error.message)
        );
      }, 450);
      if (ack && !ack.handled) ack(null, { ok: true });
    } catch (error) {
      if (ack && !ack.handled) ack(error);
    }
  });
};
