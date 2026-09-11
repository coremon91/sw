# SW — Software Video Switcher

Vizrt TriCaster의 제작 흐름을 참고하는 **Windows C++20 / Qt 6 / Direct3D 11** 소프트웨어 비디오 스위처입니다. 현재는 실제 영상 합성 엔진과 제조사 카드 I/O 어댑터를 포함하는 **0.1 개발 버전**입니다.

## 구현

- 동시 입력 슬롯 4개, 서로 다른 PGM/PVW 출력 2개.
- HD 1080i29.97 (59.94 fields/s) 또는 UHD 2160p59.94 세션.
- CUT, 디졸브, PVW에서 편집하는 PIP DVE: 크기·위치·크롭·회전·불투명도·테두리.
- DeckLink SDI 캡처/예약 재생, AJA NTV2 AutoCirculate 캡처/재생.
- 장치 검색, 포트 검증, 신호 소실 표시, 드롭·처리 시간 진단.
- 테스트 패턴 모드와 실제 UHD SDI 프레임 마커 루프백 진단.
- **INPUT 4 내장 플레이어**: 기존 플레이어의 FFmpeg 런타임으로 파일을 직접 입력. CUE·재생·일시정지·탐색·반복, CH1/2 오디오와 PGM/PVW 연결. [사용법과 포맷 조건](docs/08-internal-player.ko.md).
- **플레이어 탭**: 지정한 KBC Tech Player RTX 5060 / 2026-09-07 원본 화면을 탭 안에서 사용. 원본의 SDI 출력 방식을 유지하며, 이전 INPUT 4 파일 입력 기능도 선택 가능합니다. [원본 플레이어 탭 사용법](docs/09-original-player-tab.ko.md).
- **DeckLink 수신 확인 모드**: 외부 4K 플레이어가 KONA로 출력하는 영상을 입력 1개부터 수신. KONA 점유·SDI 출력 없이 화면과 수신 프레임 수/fps 확인. `dist/SW/DeckLink-receive.cmd`로 설정 화면을 엽니다.
- **KONA 5 수신 확인 모드**: 반대로 DeckLink 출력 → KONA 입력을 확인. `KONA-receive.cmd`로 실행하며, 입력 1–4개만 열고 SDI 출력은 생성하지 않습니다.
- 수신 모니터는 59.94회/초 처리를 목표로 하며 입력 FPS, 화면 처리 FPS, 모니터 생략 횟수를 구분합니다. UHD 축소 필터와 상태 영역 높이 고정으로 표시 안정성을 개선했습니다.

입력 4개는 현재 세션 포맷과 일치해야 합니다. HD와 UHD를 동시에 출력하지 않습니다. 현재 픽셀 경로는 **8-bit UYVY / Rec.709 SDR**이며 10-bit와 방송용 동기화는 후속 작업입니다. 실제 장시간 운용 검증 완료 여부는 별도로 기록합니다.

## 실행 및 개발

빌드된 프로그램은 `dist/SW/sw_switcher.exe`입니다. 같은 폴더의 Qt DLL, Visual C++ x64 런타임과 카드 드라이버가 필요합니다.

```powershell
.\scripts\bootstrap.ps1
.\scripts\prepare-player.ps1 -PlayerRoot '기존 KONA 플레이어 소스 폴더'
.\scripts\build.ps1
.\scripts\run.ps1 -Demo
```

개발 환경은 Windows x64, Visual Studio의 C++ 데스크톱 개발 워크로드, Python 3, Git입니다. Qt 6.8.3과 고정 리비전의 AJA SDK 준비 스크립트를 제공합니다.

- [실행 방법·현재 기능·제약·루프백 시험](docs/03-implementation.ko.md)
- [최초 조건 검토](docs/01-feasibility-review.ko.md)
- [개발 단계와 검증 기준](docs/02-development-plan.ko.md)
- [외부 구성요소 및 라이선스](THIRD_PARTY_NOTICES.md)
- [실제 검증 결과와 UHD 성능 제한](docs/04-validation-2026-09-09.ko.md)
- [양방향 수신 시험·화면 떨림 확인](docs/06-receiver-jitter.ko.md)
- [KONA 입력 → DeckLink 출력 오디오 점검](docs/07-audio-validation.ko.md)

## 버전 관리

[coremon91/sw](https://github.com/coremon91/sw)에서 관리합니다. 기능은 `codex/<작업명>` 브랜치로 개발하고 검증 내용을 PR에 남깁니다. GitHub Actions는 플랫폼 독립 코어 시험을 실행합니다.

`native/`는 실제 구현, `tests/`는 엔진·포트 검사, `scripts/`는 빌드 도구입니다. 기존 `index.html`과 `src/`는 웹 시안으로 보존합니다. SDK 설치물, 빌드 산출물과 캡처 영상은 커밋에서 제외합니다.
