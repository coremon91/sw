"""Install the reviewed CG frontend; preserve extension code and all Replicant data."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import shutil
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else 'Z:/Nodecg').resolve()
source = Path(__file__).resolve().parents[1] / 'integrations/nodecg/broadcast'
bundle = root / 'bundles/my-cg'
(root / 'assets/my-cg/program-head').mkdir(parents=True, exist_ok=True)
if not (bundle / 'graphics/sw-composite.html').is_file():
    raise SystemExit('Install the SW internal CG adapter first.')
files = sorted(p for p in source.rglob('*') if p.is_file())
if not files:
    raise SystemExit('Broadcast layout files are missing.')
for file in files:
    relative = file.relative_to(source)
    if relative.parts[0] not in ('graphics', 'dashboard') or 'emergency' in file.name:
        raise SystemExit(f'Unexpected layout file: {relative}')
protected = sorted([*bundle.glob('graphics/emergency.*'), *bundle.glob('dashboard/emergency.js'), *bundle.glob('extension/*.js')])
writes = {file.relative_to(source): file.read_bytes() for file in files}
# Add the common controls to both maintained and legacy dashboard panels.
# Only the script tag is added to legacy HTML; graphics and behavior scripts stay intact.
numeric_script = '<script src="numeric-controls.js"></script>'
for name in ('logo', 'weather', 'lower-third', 'emergency', 'election'):
    relative = Path('dashboard') / (name + '.html')
    data = writes.get(relative)
    if data is None and (bundle / relative).is_file(): data = (bundle / relative).read_bytes()
    if data is not None:
        html = data.decode('utf-8')
        if numeric_script not in html: html = html.replace('</body>', numeric_script + '\n</body>')
        writes[relative] = html.encode('utf-8')
# Register the full-page studio after its assets; no extension code changes.
if (source / 'dashboard/studio.html').exists():
    for name in ('sw-composite.html', 'sw-composite.js'):
        writes[Path('graphics') / name] = (source.parent / name).read_bytes()
    package = json.loads((bundle / 'package.json').read_text(encoding='utf-8-sig'))
    panels = package['nodecg']['dashboardPanels']
    studio = {'name': 'studio-control', 'title': '통합 CG 스튜디오', 'file': 'studio.html', 'fullbleed': True}
    package['nodecg']['dashboardPanels'] = [studio] + [p for p in panels if p['name'] != 'studio-control']
    writes[Path('package.json')] = (json.dumps(package, ensure_ascii=False, indent=2) + '\n').encode('utf-8')
def sha(file): return hashlib.sha256(file.read_bytes()).hexdigest()
before = {str(p.relative_to(bundle)): sha(p) for p in protected}
changed = [relative for relative, data in writes.items() if not (bundle / relative).is_file() or data != (bundle / relative).read_bytes()]
if not changed:
    print('Broadcast layout already installed.'); sys.exit(0)
backup = root / 'logs' / ('broadcast-layout-backup-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
backup.mkdir(parents=True)
manifest = {'changed': [], 'protectedSha256': before}
# Finish every backup before modifying any served asset.
for relative in changed:
    destination = bundle / relative
    manifest['changed'].append({'path': relative.as_posix(), 'existed': destination.exists()})
    if destination.exists():
        saved = backup / relative; saved.parent.mkdir(parents=True, exist_ok=True); shutil.copy2(destination, saved)
(backup / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
for relative in changed:
    destination = bundle / relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(destination.name + '.sw-layout.tmp')
    temporary.write_bytes(writes[relative]); temporary.replace(destination)
for file in protected:
    if sha(file) != before[str(file.relative_to(bundle))]: raise SystemExit(f'Protected file changed: {file}')
print(json.dumps({'installed': len(changed), 'backup': str(backup), 'emergency_graphics_and_extensions_unchanged': True}, ensure_ascii=False))
