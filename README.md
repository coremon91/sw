# SW — Z 드라이브 운영 폴더 기준 보관본

2026-10-21 기준 다음 세 폴더의 실제 파일을 기준으로 정리했습니다.

| 폴더 | 용도 | 저장소에 포함된 내용 |
|---|---|---|
| `SW/` | `Z:\SW\SW` 개발 작업 폴더 | 이 폴더에서 Git으로 관리하던 소스·빌드 스크립트·문서 54개 |
| `SW-integrated-20260924/` | 통합 스위처 실행 배포본 | 실행 CMD, 사용 문서, 라이선스 |
| `SW-internal-CG-20260924/` | 내부 CG 실행 배포본 | 실제 실행 CMD, 사용 문서, NodeCG 스크립트·그래픽 자산, 라이선스 |

위 폴더 안의 파일은 Z: 원본을 그대로 복사했습니다. 파일별 SHA-256은 `snapshot-files.json`에 기록했습니다. 특히 내부 CG 실행 CMD의 `sw_switcher-studio-uhd5994.exe` 대상도 원본과 동일합니다.

`SW/`의 소스는 원본 Git 커밋 `03d3e387bc4db0523bc8097cecff200f534da98e`에 해당합니다. 두 실행 배포본의 EXE가 이 소스에서 빌드됐다고 보장하지 않습니다. 내부 CG 관련 최신 개발 소스가 담긴 이전 커밋 `887ef58c23e6e2eb43da0c71eb4fccdcb6acc0a4`은 Git 이력에 그대로 남아 있습니다.

## 전체 폴더 복원용 백업

소스 ZIP 다운로드만으로는 실행 배포본 전체가 복원되지 않습니다. EXE, DLL, 리소스, 기존 백업과 개발 의존성을 포함한 전체 보관본은 다음과 같이 별도로 준비했습니다.

| 압축 파일 | 포함 파일 수 | 압축 크기(바이트) |
|---|---:|---:|
| `SW.zip` | 12,306 | 549,252,679 |
| `SW-integrated-20260924.zip` | 162 | 166,082,405 |
| `SW-internal-CG-20260924.zip` | 282 | 177,626,735 |

현재 상태: **GitHub Releases 전체 백업 업로드 및 SHA-256 일치 확인 완료**.

[전체 백업 다운로드 — z-sw-backup-20261021](https://github.com/coremon91/sw/releases/tag/z-sw-backup-20261021)

릴리스의 **Assets**에서 `SW.zip`, `SW-integrated-20260924.zip`, `SW-internal-CG-20260924.zip`을 받으세요. GitHub가 자동 생성하는 `Source code (zip)`은 전체 실행본이 아닙니다.

전체 ZIP은 각 원본 폴더의 파일을 포함하며 `SW/.git/`만 제외했습니다. 각 ZIP의 SHA-256은 `SHA256SUMS.txt`에 있습니다. 원본의 모든 파일 경로·크기·SHA-256을 담은 `*.manifest.json`도 릴리스에 함께 첨부했습니다.

복원할 때는 해당 ZIP을 `Z:\SW`에 풀어 동일한 폴더 구조로 사용합니다. SDK·드라이버·Visual C++ 런타임 등 시스템에 별도로 설치되는 항목은 파일 복원과 별개입니다.

## 소스 빌드

개발 소스의 안내는 [SW/README.md](SW/README.md)를 참고합니다. 코어 테스트는 저장소 루트에서 다음과 같이 실행할 수 있습니다.

```sh
cmake -S SW -B build -DSW_BUILD_DESKTOP=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```
