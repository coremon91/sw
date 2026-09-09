# SW — Software Video Switcher

Vizrt TriCaster의 제작 흐름을 참고하는 **Windows C++20 / Qt 6 / Direct3D 11** 소프트웨어 비디오 스위처입니다. 현재는 실제 영상 합성 엔진과 제조사 카드 I/O 어댑터를 포함하는 **0.1 개발 버전**입니다.

## 구현

- 동시 입력 슬롯 4개, 서로 다른 PGM/PVW 출력 2개.
- HD 1080i29.97 (59.94 fields/s) 또는 UHD 2160p59.94 세션.
- CUT, 디졸브, PVW에서 편집하는 PIP DVE: 크기·위치·크롭·회전·불투명도·테두리.
- DeckLink SDI 캡처/예약 재생, AJA NTV2 AutoCirculate 캡처/재생.
- 장치 검색, 포트 검증, 신호 소실 표시, 드롭·처리 시간 진단.
- 테스트 패턴 모드와 실제 UHD SDI 프레임 마커 루프백 진단.

입력 4개는 현재 세션 포맷과 일치해야 합니다. HD와 UHD를 동시에 출력하지 않습니다. 현재 픽셀 경로는 **8-bit UYVY / Rec.709 SDR**이며 10-bit와 방송용 동기화는 후속 작업입니다. 실제 장시간 운용 검증 완료 여부는 별도로 기록합니다.

## 실행 및 개발

빌드된 프로그램은 `dist/SW/sw_switcher.exe`입니다. 같은 폴더의 Qt DLL, Visual C++ x64 런타임과 카드 드라이버가 필요합니다.

```powershell
.\scripts\bootstrap.ps1
.\scripts\build.ps1
.\scripts\run.ps1 -Demo
```

개발 환경은 Windows x64, Visual Studio의 C++ 데스크톱 개발 워크로드, Python 3, Git입니다. Qt 6.8.3과 고정 리비전의 AJA SDK 준비 스크립트를 제공합니다.

- [실행 방법·현재 기능·제약·루프백 시험](docs/03-implementation.ko.md)
- [최초 조건 검토](docs/01-feasibility-review.ko.md)
- [개발 단계와 검증 기준](docs/02-development-plan.ko.md)
- [외부 구성요소 및 라이선스](THIRD_PARTY_NOTICES.md)
- [실제 검증 결과와 UHD 성능 제한](docs/04-validation-2026-09-09.ko.md)

## 버전 관리

[coremon91/sw](https://github.com/coremon91/sw)에서 관리합니다. 기능은 `codex/<작업명>` 브랜치로 개발하고 검증 내용을 PR에 남깁니다. GitHub Actions는 플랫폼 독립 코어 시험을 실행합니다.

`native/`는 실제 구현, `tests/`는 엔진·포트 검사, `scripts/`는 빌드 도구입니다. 기존 `index.html`과 `src/`는 웹 시안으로 보존합니다. SDK 설치물, 빌드 산출물과 캡처 영상은 커밋에서 제외합니다.
