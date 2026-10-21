"""Local-only visual fixture: uses real graphics, never connects to NodeCG."""
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
import json
import mimetypes

STAGE = Path(__file__).resolve().parents[1] / 'integrations/nodecg/broadcast'
ORIGINAL = Path('Z:/Nodecg/bundles/my-cg')
DATA = {
    'naturalDisaster': {'visible': False, 'label': '자연재해', 'message': '[시험] 자연재해 스크롤 검증용 문구입니다.', 'speed': 110},
    'programHead': {'visible': True, 'line1': 'MORNING', 'line2': 'WIDE'},
    'lowerThird': {'visible': True, 'name': '광주·전남의 아침, KBC 모닝와이드', 'title': ''},
    'logo': {'visible': True, 'image': '/bundles/my-cg/graphics/images/logo.png', 'x': 92, 'y': 865, 'width': 150, 'height': 150},
    'weather': {'visible': True, 'items': [{'city': '광주', 'temp': '21.7℃', 'condition': '맑음'}, {'city': '목포', 'temp': '20.3℃', 'condition': '비'}], 'intervalSeconds': 2},
    'emergencyTicker': {'visible': False, 'level': '재난속보', 'message': '재난 CG 유지 확인용 시험 문구입니다.', 'source': 'KBC 재난방송', 'speed': 170, 'bottom': 42, 'fontSize': 58, 'theme': 'danger'},
}
FIXTURE = '''const fixtureData = %s;
const reps = new Map();
window.nodecg={Replicant(name){if(!reps.has(name))reps.set(name,{value:fixtureData[name],handlers:[],on(event,fn){this.handlers.push(fn);fn(this.value);}});return reps.get(name);}};
window.addEventListener('message',event=>{if(event.origin!==location.origin||event.data?.type!=='sw-preview')return;for(const [name,value] of Object.entries(event.data.values)){const rep=reps.get(name);if(rep){rep.value=value;rep.handlers.forEach(fn=>fn(value));}}});
''' % json.dumps(DATA, ensure_ascii=False)
PAGE = '''<!doctype html><html lang="ko"><meta charset="utf-8"><title>SW 방송 CG 미리보기</title>
<style>body{margin:0;background:#101722;color:#f5f6f8;font:14px 'Malgun Gothic',sans-serif}header{padding:20px}h1{margin:0 0 12px;font-size:22px}button{padding:9px 15px;margin-right:8px;border:0;border-radius:4px;cursor:pointer}#viewport{width:100%;aspect-ratio:16/9;position:relative;overflow:hidden}#scene{width:1920px;height:1080px;position:absolute;transform-origin:0 0;background:linear-gradient(140deg,#2e4558,#111d30)}iframe{position:absolute;inset:0;border:0;width:1920px;height:1080px;pointer-events:none}.guide{position:absolute;left:104px;top:340px;color:#ffffff50;font-size:50px;letter-spacing:5px}p{color:#a7b4c4}</style>
<header><h1>SW · 모닝와이드 CG</h1><button id="emergency">재난 함께 표시</button><button id="long">긴 자막 시험</button><button id="out">전체 OUT / TAKE</button><p>검증용 가상 배경 · 시험 데이터 / 실제 송출 연결 없음</p></header>
<div id="viewport"><div id="scene"><div class="guide">영상 영역 / CG 투명 합성</div>
<iframe title="하단 자막" src="/bundles/my-cg/graphics/lower-third.html"></iframe><iframe title="재난" src="/bundles/my-cg/graphics/emergency.html"></iframe><iframe title="로고" src="/bundles/my-cg/graphics/logo.html"></iframe><iframe title="날씨" src="/bundles/my-cg/graphics/weather.html"></iframe></div></div>
<script>const values=__FIXTURE_DATA__;let on=true;
function push(){document.querySelectorAll('iframe').forEach(f=>f.contentWindow.postMessage({type:'sw-preview',values},location.origin));}
document.getElementById('emergency').onclick=()=>{values.emergencyTicker.visible=!values.emergencyTicker.visible;push();};
document.getElementById('long').onclick=()=>{values.lowerThird.name='광주·전남 지역의 오늘 소식과 주요 현안을 시민 여러분께 정확하고 신속하게 전해드립니다';push();};
document.getElementById('out').onclick=()=>{on=!on;for(const name of ['programHead','lowerThird','logo','weather'])values[name].visible=on;push();};
new ResizeObserver(()=>document.getElementById('scene').style.transform=`scale(${document.getElementById('viewport').clientWidth/1920})`).observe(document.getElementById('viewport'));
</script></html>'''.replace('__FIXTURE_DATA__', json.dumps(DATA, ensure_ascii=False))

PAGE = PAGE.replace('#viewport{width:100%;', '#viewport{width:100%;max-width:940px;margin:auto;')

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        route = self.path.split('?')[0]
        if route == '/':
            data, mime = PAGE.encode(), 'text/html; charset=utf-8'
        elif route == '/fixture.js':
            data, mime = FIXTURE.encode(), 'application/javascript; charset=utf-8'
        elif route.startswith('/bundles/my-cg/'):
            relative = Path(route.removeprefix('/bundles/my-cg/'))
            if '..' in relative.parts:
                self.send_error(403); return
            file = STAGE / relative
            if not file.is_file(): file = ORIGINAL / relative
            if not file.is_file(): self.send_error(404); return
            data = file.read_bytes(); mime = mimetypes.guess_type(file)[0] or 'application/octet-stream'
            if file.suffix == '.html': data = data.replace(b'<head>', b'<head><script src="/fixture.js"></script>')
        else:
            self.send_error(404); return
        self.send_response(200); self.send_header('Content-Type', mime); self.send_header('Content-Length', str(len(data))); self.send_header('Cache-Control', 'no-store'); self.end_headers(); self.wfile.write(data)

if __name__ == '__main__':
    HTTPServer(('127.0.0.1', 9092), Handler).serve_forever()
