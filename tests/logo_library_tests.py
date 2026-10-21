"""Validate delivered image/animation files (Pillow required)."""
from pathlib import Path
from PIL import Image, ImageSequence
import json

root = Path(__file__).resolve().parents[1] / 'integrations/nodecg/broadcast/graphics/logo-library'
catalog = json.loads((root/'catalog.json').read_text(encoding='utf-8'))
report = json.loads((root/'import-report.json').read_text(encoding='utf-8'))
assert len(report['files']) == catalog['sourceFileCount'] == 433
assert all(item['status'] != 'unsupported' for item in report['files'].values())
assert len({item['id'] for item in catalog['entries']}) == 23
for item in catalog['entries']:
    with Image.open(root/Path(item['image']).name) as image:
        frames = list(ImageSequence.Iterator(image)) if item['kind']=='still' else None
        if item['kind']=='animation':
            total, visible, distinct = 0, False, set()
            for frame in ImageSequence.Iterator(image):
                rgba=frame.convert('RGBA')
                total += frame.info.get('duration', 0)
                visible |= rgba.getchannel('A').getextrema()[1] > 100
                distinct.add(hash(rgba.tobytes()))
            assert abs(total-round(item['frames']*1000/item['fps'])) <= 1, (item['label'],total)
            assert visible and len(distinct)>1, item['label']
            assert image.info['loop']==0
        else:
            alpha=image.convert('RGBA').getchannel('A')
            assert alpha.getbbox(), item['label']
            if any(source.lower().endswith('.tif') for source in item['sources']):
                assert alpha.getextrema()==(0,255), 'TIFF alpha was lost'
    with Image.open(root/Path(item['thumbnail']).name) as thumb:
        assert thumb.width<=280 and thumb.height<=120
print('PASS: 433 indexed files, 23 distinct assets, TIFF alpha, WebP frame variation/loop/duration, thumbnails.')
