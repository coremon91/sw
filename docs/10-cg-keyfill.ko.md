# SW 0.2 · CG Key / Fill 합성

## 연결과 사용

기존 4개 입력 슬롯을 사용합니다. 기본 예: INPUT 1/2 = 배경 영상,
INPUT 3 = CG Fill, INPUT 4 = CG Key. 별도 입력 2개를 추가한 6입력 구조는 아닙니다.
SDI ROUTING에서 실제 카드 포트를 IN 1~4에 지정하세요. DeckLink 4채널을
사용하려면 카드의 4개 독립 하위 장치 프로파일이 필요합니다.

1. CG 장비의 Fill과 Key를 각각 독립 SDI 입력에 연결합니다.
2. 모든 입력을 세션 포맷(1080i29.97 또는 2160p59.94)에 맞춥니다.
3. 세션을 시작하고 INPUT 1 또는 2를 PVW 배경으로 선택합니다.
4. CG KEY / FILL 패널에서 Fill/Key 입력을 선택하고 CG 합성 사용을 켭니다.
5. PVW에서 결과를 확인한 뒤 CUT 또는 AUTO로 PGM에 반영합니다.
6. CG를 내리려면 PVW에서 합성을 끈 장면을 준비한 뒤 CUT/AUTO 합니다.
   CUT은 PGM과 PVW 장면 전체를 교환하므로 이후 PVW의 CG 상태도 확인하세요.

Key 검정은 배경, 흰색은 Fill, 중간 밝기는 반투명입니다. Key 반전도 지원합니다.
기본 Straight Fill: 배경*(1-Key) + Fill*Key.
Premultiplied Fill: 배경*(1-Key) + Fill. CG 출력 방식과 일치시켜야
글자/로고 경계가 어두워지거나 밝게 뜨지 않습니다. 이 모드의 Fill은 이미
Key가 곱해진 영상이어야 합니다. 불투명도는 CG 전체 강도를 조절합니다.
Key와 Fill은 서로 다른 입력이어야 하며, 둘 중 하나라도 신호가 없거나
500ms 이상 갱신되지 않으면 CG를 숨기고 배경/PIP만 표시합니다.

## 범위와 제약

CG는 DVE/PIP 위에 전체 화면 크기로 합성됩니다. Key는 Rec.709 제한 범위
8-bit UYVY 신호의 밝기로 해석합니다. CG 오디오는 배경에 합산하지 않으며
기존 배경 영상의 오디오 선택 동작을 유지합니다. 설정은 세션 시작 시 초기화됩니다.
Fill과 Key의 프레임 번호를 맞추는 전용 프레임 싱크/지연 보정은 없습니다.
CG 장비에서 동기된 동일 포맷의 Fill/Key를 보내야 합니다. 실제 SDI 지연과
경계 정합은 장비를 연결해 확인해야 합니다. 10-bit/HDR 및 혼합 포맷은 미지원입니다.

## 검증

sw_bench --keyfill: HD/UHD GPU 픽셀 검사(검정/흰색/회색, 반전, 불투명도,
Premultiplied, 신호 소실, 공간별 매트, 두 HD 필드, CUT/AUTO).
물리 SDI CG 장비 연동과 장시간 부하 시험은 별도입니다.
