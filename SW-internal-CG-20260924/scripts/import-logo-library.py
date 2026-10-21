"""Convert a read-only broadcast logo archive to browser-safe, alpha-preserving assets.

Requires Pillow with WebP support. Tornado project files are indexed as references,
not misrepresented as browser-renderable scenes. TAD companions use their TGA sequence.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
from PIL import Image


def read_still(path):
    with Image.open(path) as image:
        tags = getattr(image, 'tag_v2', {})
        # These TIFFs have an unspecified fourth sample, which Pillow otherwise drops.
        if (tags.get(277) == 4 and tags.get(259) == 1
                and tuple(tags.get(258, ())) == (8, 8, 8, 8)
                and tags.get(284, 1) == 1 and tags.get(274, 1) == 1):
            data = path.read_bytes()
            pixels = b''.join(data[o:o+n] for o, n in zip(tags[273], tags[279]))
            if len(pixels) != image.width * image.height * 4:
                raise ValueError(f'Unexpected TIFF strip size: {path}')
            return Image.frombytes('RGBA', image.size, pixels)
        return image.convert('RGBA')


def read_vrv(path):
    data = path.read_bytes()
    if data[:4] != b'vrv\0' or data[12] != 32:
        raise ValueError(f'Unsupported VRV header: {path}')
    width, height = struct.unpack_from('<II', data, 4)
    fps, count = data[13], struct.unpack_from('<I', data, 18)[0]
    if not (0 < width <= 4096 and 0 < height <= 2160 and 0 < count <= 5000 and fps):
        raise ValueError('Invalid VRV dimensions or frame count')
    frames = []
    for index in range(count):
        offset, size = struct.unpack_from('<QQ', data, 22 + index * 36 + 8)
        if offset < 22 + count * 36 or offset + size > len(data):
            raise ValueError('Invalid VRV frame offset')
        source, pixels, cursor = data[offset:offset+size], bytearray(), 0
        while cursor < len(source):
            packet = source[cursor]
            cursor += 1
            run = (packet & 127) + 1
            length = 4 if packet & 128 else run * 4
            if cursor + length > len(source):
                raise ValueError('Truncated VRV packet')
            pixels.extend(source[cursor:cursor+length] * (run if packet & 128 else 1))
            cursor += length
            if len(pixels) > width * height * 4:
                raise ValueError('Oversized VRV frame')
        # This archive's UHD exporter wraps a final 129-pixel transparent run
        # to 0x80 (one pixel). Repair only that exact terminal pattern. The
        # resulting alpha is checked against the supplied PNGs 308 and 310.
        if len(pixels) == (width * height - 128) * 4 and source[-5] == 128 and source[-1] == 0:
            pixels.extend(source[-4:] * 128)
        if len(pixels) != width * height * 4:
            raise ValueError('Incomplete VRV frame')
        frame = Image.frombytes('RGBA', (width, height), bytes(pixels), 'raw', 'BGRA')
        companion = path.with_name(f'{path.stem}_{index:05}.png')
        if companion.exists():
            reference = read_still(companion)
            if frame.getchannel('A').tobytes() != reference.getchannel('A').tobytes():
                raise ValueError('VRV alpha does not match companion PNG')
        frames.append(frame)
    return frames, fps


def build(source, output):
    output.mkdir(parents=True, exist_ok=True)
    files = sorted(p for p in source.rglob('*') if p.is_file())
    entries, records, by_hash = [], {}, {}
    base = '/bundles/my-cg/graphics/logo-library/'
    def relative(p): return p.relative_to(source).as_posix()
    def add(paths, frames, fps=0, label=None, note=''):
        digest = hashlib.sha256(b''.join(hashlib.sha256(p.read_bytes()).digest() for p in paths)).hexdigest()[:16]
        if digest in by_hash:
            entry = by_hash[digest]
            entry['sources'].extend(relative(p) for p in paths)
        else:
            boxes = [f.getchannel('A').getbbox() for f in frames]
            boxes = [b for b in boxes if b]
            box = (min(b[0] for b in boxes), min(b[1] for b in boxes), max(b[2] for b in boxes), max(b[3] for b in boxes)) if boxes else (0, 0, *frames[0].size)
            cropped = [f.crop(box) for f in frames]
            animated = len(frames) > 1
            name = digest + ('.webp' if animated else '.png')
            if animated:
                durations = [round((i+1)*1000/fps)-round(i*1000/fps) for i in range(len(frames))]
                cropped[0].save(output/name, save_all=True, append_images=cropped[1:], duration=durations, loop=0, lossless=True, method=4, exact=True)
            else:
                cropped[0].save(output/name)
            thumb = max(cropped, key=lambda f: sum(f.getchannel('A').histogram()[128:])).copy()
            thumb.thumbnail((280, 120))
            thumb.save(output/(digest+'-thumb.png'))
            width, height = cropped[0].size
            scale = min(1, 800/width, 800/height)
            if frames[0].size == (3840, 2160): scale = min(scale, .5)
            if frames[0].size == (1920, 1080): scale = min(scale, 800/width)
            defaults = {'width': round(width*scale), 'height': round(height*scale)}
            # Normal UHD corner logos retain their original on-screen placement.
            if frames[0].size == (3840, 2160) and width <= 1600 and height <= 1600:
                defaults.update(x=round(box[0]/2), y=round(box[1]/2))
            entry = {'id': digest, 'label': label or paths[0].stem, 'kind': 'animation' if animated else 'still',
                     'image': base+name, 'thumbnail': base+digest+'-thumb.png', 'sources': [relative(p) for p in paths],
                     'size': list(frames[0].size), 'crop': list(box), 'frames': len(frames), 'fps': fps,
                     'defaults': defaults, 'note': note}
            entries.append(entry)
            by_hash[digest] = entry
        for p in paths: records[relative(p)] = {'status': 'converted', 'entry': entry['id']}
        return entry

    sequences = {}
    for p in files:
        if p.suffix.lower() == '.tga' and re.search(r'_\d{3,}$', p.stem):
            sequences.setdefault(p.parent, []).append(p)
    for folder, paths in sequences.items():
        paths.sort()
        frames = [read_still(p) for p in paths]
        vrv = next(folder.glob('*.vrv'), None)
        if vrv:
            decoded, fps = read_vrv(vrv)
            if len(decoded) != len(frames) or any(a.tobytes() != b.tobytes() for a, b in zip(decoded, frames)):
                raise ValueError('VRV does not match its companion TGA frames')
            # Keep one full animation entry; map every verified TGA frame to it.
            entry = add([vrv], decoded, fps, 'KBC + SBS LIVE · 애니메이션')
            entry['verifiedTgaFrames'] = len(paths)
            for p in paths: records[relative(p)] = {'status': 'verified-animation-frame', 'entry': entry['id']}
        else:
            entry = add(paths, frames, 30, 'KBC + SBS LIVE · 후반 반복 (h)')
        for tad in folder.glob('*.tad'):
            records[relative(tad)] = {'status': 'companion-sequence', 'entry': entry['id'], 'note': 'TAD 자체 디코딩이 아닌 같은 폴더 TGA 시퀀스 사용; TAD 타이밍은 미검증'}
    for p in files:
        if relative(p) in records: continue
        ext = p.suffix.lower()
        if ext in ('.png', '.tif', '.tiff', '.tga'):
            add([p], [read_still(p)])
        elif ext == '.vrv':
            frames, fps = read_vrv(p)
            add([p], frames, fps, 'KBC + UHD · 애니메이션' if 'UHD' in p.stem.upper() else 'KBC + SBS LIVE · 애니메이션')
        elif ext in ('.t2p', '.t2s', '.t2st'):
            data = p.read_bytes()
            refs = sorted(set(s for alignment in (0, 1) for s in re.findall(r'[\x20-\x7e가-힣]{5,}', data[alignment:].decode('utf-16-le', errors='ignore')) if re.search(r'\.(?:tga|tif|png|vrv|t2s|t2st)$', s, re.I)))
            records[relative(p)] = {'status': 'project-reference', 'references': refs, 'note': '편집 프로젝트는 원본 이미지/애니메이션으로 선택; 프로젝트 효과·좌표는 직접 재생하지 않음'}
        else:
            records[relative(p)] = {'status': 'unsupported'}
    for entry in entries:
        if entry['label'] in ('SBS UHD', 'SBS UHD LIVE', '자막 가이드'):
            entry['note'] = '넓은 캔버스에 픽셀이 있는 원본입니다. 전체 내용을 보존해 등록했습니다.'
    warnings = ['Tornado 편집 파일(T2P/T2S/T2ST)은 직접 재생하지 않습니다. 함께 있는 이미지·애니메이션을 등록했으며 편집 효과·장면 배치는 포함하지 않습니다.',
                'TAD 2개는 같은 폴더의 TGA 연속 프레임으로 제공합니다. 원본 TAD의 타이밍은 미검증입니다.']
    if not any(p.name.lower() == 'kbc sbs uhd hdr.tga' for p in files):
        warnings.append('HDR 장면이 참조하는 kbc sbs uhd hdr.tga 원본이 없어 해당 HDR 로고는 미등록입니다.')
    catalog = {'version': 1, 'entries': entries, 'warnings': warnings, 'sourceFileCount': len(files)}
    (output/'catalog.json').write_text(json.dumps(catalog, ensure_ascii=False, indent=2), encoding='utf-8')
    (output/'import-report.json').write_text(json.dumps({'source': str(source), 'files': records, 'warnings': warnings}, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'entries': len(entries), 'files': len(files), 'animations': sum(e['kind']=='animation' for e in entries)}, ensure_ascii=False))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    build(args.source.resolve(), args.output.resolve())
