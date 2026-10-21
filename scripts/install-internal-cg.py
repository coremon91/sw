"""Install the small SW adapter into the existing my-cg bundle, with backups."""
import argparse
import json
from pathlib import Path
from datetime import datetime

parser = argparse.ArgumentParser()
parser.add_argument("root", type=Path)
args = parser.parse_args()
root = args.root.resolve(strict=True)
bundle = root / "bundles/my-cg"
source = Path(__file__).resolve().parent.parent / "integrations/nodecg"
extension = bundle / "extension/index.js"
text = extension.read_text(encoding="utf-8-sig")
if 'require("./sw-internal")' not in text:
    changes = {
        'module.exports = function (nodecg) {':
            'module.exports = function (nodecg) {\n  const internalCg = require("./sw-internal")(nodecg);',
        '  function connect() {':
            '  function connect() {\n    if (internalCg) { setStatus(true, "SW 내부 CG · CasparCG 사용 안 함"); return; }',
        '  function send(command) {':
            '  function send(command) {\n    if (internalCg) return Promise.resolve();',
    }
    for before, after in changes.items():
        if text.count(before) != 1:
            raise SystemExit(f"Unexpected my-cg extension layout: {before}")
        text = text.replace(before, after)

manifest_path = bundle / "package.json"
manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
graphics = manifest["nodecg"]["graphics"]
if not any(item["file"] == "sw-composite.html" for item in graphics):
    graphics.append({"file": "sw-composite.html", "width": 1920, "height": 1080})
schema_path = bundle / "configschema.json"
schema = json.loads(schema_path.read_text(encoding="utf-8-sig"))
schema["properties"]["playoutMode"] = {
    "type": "string", "enum": ["internal", "casparcg"], "default": "casparcg"
}
config_path = root / "cfg/my-cg.json"
config = json.loads(config_path.read_text(encoding="utf-8-sig")) if config_path.exists() else {}
config["playoutMode"] = "internal"
dump = lambda value: json.dumps(value, ensure_ascii=False, indent=2) + "\n"
writes = {
    extension: text,
    manifest_path: dump(manifest),
    schema_path: dump(schema),
    config_path: dump(config),
    bundle / "extension/sw-internal.js": (source / "sw-internal.js").read_text(encoding="utf-8"),
    bundle / "graphics/sw-composite.html": (source / "sw-composite.html").read_text(encoding="utf-8"),
    bundle / "graphics/sw-composite.js": (source / "sw-composite.js").read_text(encoding="utf-8"),
}
for relative in ["dashboard/weather.js", "dashboard/logo.js", "dashboard/weather.html", "dashboard/lower-third.html"]:
    path = bundle / relative
    content = path.read_text(encoding="utf-8-sig")
    content = content.replace(" · CasparCG 1-14", "").replace(" · CasparCG 1-13", "")
    content = content.replace("CasparCG 상태 확인 중…", "CG 출력 상태 확인 중…")
    writes[path] = content
# Validate and prepare the entire change before touching the installation.
backup = root / "logs" / ("sw-internal-backup-" + datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
for path, content in writes.items():
    if path.exists() and path.read_text(encoding="utf-8-sig") == content:
        continue
    if path.exists():
        target = backup / path.relative_to(root)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(path.read_bytes())
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".sw-tmp")
    temporary.write_text(content, encoding="utf-8", newline="\n")
    temporary.replace(path)
print(f"Installed internal CG adapter. Backups: {backup}")
