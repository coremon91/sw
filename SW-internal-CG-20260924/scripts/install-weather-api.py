"""Install weather key controls' backend; takes effect on the next NodeCG start."""
from pathlib import Path
from datetime import datetime
import shutil
import sys

root=Path(sys.argv[1] if len(sys.argv)>1 else 'Z:/Nodecg').resolve()
source=Path(__file__).resolve().parents[1]/'integrations/nodecg/weather-service.js'
target=root/'bundles/my-cg/extension/weather.js'
if not target.exists(): raise SystemExit('Existing weather extension not found')
if target.read_bytes()==source.read_bytes():
    print('Weather API backend already installed.');sys.exit(0)
backup=root/'logs'/('weather-api-backup-'+datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
backup.mkdir(parents=True)
shutil.copy2(target,backup/'weather.js')
temporary=target.with_suffix('.js.tmp');temporary.write_bytes(source.read_bytes());temporary.replace(target)
print('Weather API backend installed; NodeCG restart required. Backup: '+str(backup))
