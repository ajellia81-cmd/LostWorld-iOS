# iOS 포팅 (iPhone / iPad)

The Lost World를 iPhone에서 네이티브로 실행해요. 윈도우판과 같은 재컴파일 코드와 Vulkan 렌더러를 쓰고,
Vulkan은 MoltenVK를 거쳐 Metal로 돌아가요. 서명하지 않은 `LostWorld.ipa`를 만들기 때문에
**LiveContainer**로 바로 설치할 수 있어요.

## 빌드 (Mac)

필요한 것: Xcode(한 번 실행해서 iOS SDK 설치), Homebrew의 `cmake`, `ninja`, `python`, 그리고 `lostwsga.zip`.

```
brew install cmake ninja python
platform/ios/build.sh /경로/lostwsga.zip
```

결과물은 `out/ios/LostWorld.ipa`예요. 스크립트가 하는 일은 다음과 같아요.

1. ROM을 풀고 PowerPC 코드를 C++로 재컴파일해요 (`build.bat`와 같은 과정).
2. Mac용 glslang과 m68kmake를 빌드해요. 셰이더와 사운드 CPU 테이블을 만드는 데 필요해요.
3. MoltenVK(기본 v1.4.2, `MOLTENVK_VERSION`으로 변경 가능)를 받아요. 다운로드가 안 되면 정적 라이브러리
   `libMoltenVK.a`(ios-arm64) 경로를 `MOLTENVK_LIB`에 주면 돼요.
4. arm64 iOS용으로 크로스 컴파일하고, `res/`, `textures/`, 글꼴, ROM을 앱 안의 `game/` 폴더에 넣어 .ipa로 묶어요.

ROM을 앱에 넣고 싶지 않으면 `LW_NO_ROM_IN_APP=1`을 주세요. 그 경우 앱의 Documents 폴더에 `lostwsga.zip`을 직접 넣으면 돼요.
번들 ID는 `LW_BUNDLE_ID`로 바꿀 수 있어요 (기본값 `com.snicolast.lostworld`).

## GitHub에서 빌드 (Mac 없이)

이 저장소의 Actions가 GitHub의 macOS 서버에서 같은 스크립트로 .ipa를 만들어요 (`.github/workflows/ios.yml`).
이 저장소에는 소스 코드만 있어서 공개해도 돼요. ROM과 만든 앱은 **비공개 저장소**(`LostWorld-iOS-private`)에만 둬요.

1. **비공개 저장소 (한 번만)**: README를 넣어 만들고, 태그 `rom` 릴리스에 `lostwsga.zip`을 첨부해요.
2. **토큰 (한 번만)**: Fine-grained 토큰을 그 저장소에만, Contents: Read and write 권한으로 만들어
   이 저장소의 Actions 시크릿 `PRIVATE_REPO_TOKEN`에 넣어요.
3. **빌드**: Actions > iOS build > Run workflow를 누르고 `ipa`를 골라요.
4. **받기**: 비공개 저장소의 릴리스 `latest`에 `LostWorld.ipa`가 올라와요 (빌드할 때마다 바뀌어요).
   이 ipa에는 ROM이 없어요. 폰에서 앱의 Documents 안 `rom` 폴더에 `lostwsga.zip`을 한 번 넣어 두면 돼요.

`check`를 고르면 ROM 없이 iOS용으로 컴파일·링크까지만 확인해요.
앱 안에는 ROM에서 만든 게임 코드가 들어 있어서 공개된 곳에 올리면 안 돼요 (LICENSE). 그래서 결과물을 이 저장소에 올리지 않아요.

## LiveContainer로 설치

1. `LostWorld.ipa`를 AirDrop이나 파일 앱으로 아이폰에 옮겨요.
2. LiveContainer에서 **+** 를 눌러 .ipa를 추가하고 실행해요.

ROM은 앱에 들어 있지 않아요. 한 번 실행하면 데이터 폴더(앱 기준 Documents)에 `rom` 폴더가 생기니 거기에 `lostwsga.zip`을 넣고 다시 실행해요. 저장 데이터, 설정, 기록은 LiveContainer가 이 앱에 준
데이터 폴더(앱 기준 Documents)에 남아요. 여기에 `crosshair.png`를 넣으면 조준점 그림으로 쓰이고,
`textures/` 같은 텍스처 팩 폴더를 직접 만들어 넣을 수도 있어요.

## 화면

- iPhone 18 Pro Max(2868×1320, 약 2.17:1)를 기준으로 3D 화면을 가로 전체에 채워요.
  OPTIONS > DISPLAY > WIDESCREEN에서 더 좁은 값을 골라도 폰에서는 화면 폭에 맞춰요.
- 화면 위쪽 HUD 띠와 터치 버튼은 둥근 모서리와 다이내믹 아일랜드를 피해요 (iOS 안전 영역).
- 가로 모드 고정이고, ProMotion 120Hz를 써요.

## 조작

| | 터치 | 게임패드 |
|---|---|---|
| 조준 + 발사 | 화면을 누르면 그 자리를 쏘고, 누르고 있으면 계속 쏴요 (손가락을 따라 움직여요) | 스틱 또는 자이로, R2/R1 |
| 재장전 | 왼쪽 아래 RELOAD, 또는 쏘는 중에 다른 손가락으로 탭 | L2/L1 |
| 코인 + 시작 | 오른쪽 아래 START (게임 중이 아닐 때만 보여요) | START |
| 일시정지 | 왼쪽 위 ❚❚ | BACK |
| 메뉴 | 탭이 클릭이고, 왼쪽 위 BACK으로 뒤로 가요 | 십자키, A, B |

**게임패드를 연결하면 터치 버튼이 사라지고, 게임 중 화면 터치는 무시돼요.** 메뉴에서는 탭이 계속 동작해요.
패드를 빼면 터치 버튼이 다시 나타나요. iPad에 마우스를 연결하면 윈도우판처럼 마우스로 조준할 수 있어요
(왼쪽 버튼 발사, 오른쪽 버튼 재장전).

## 윈도우판과 다른 점

- **레이트레이싱 그림자/AO 없음**: MoltenVK가 레이 쿼리를 지원하지 않아요. 일반 그림자 맵은 그대로예요.
- **DLAA 없음** (NVIDIA 전용): MSAA를 써요.
- **스플래시 영상 없음**: 같은 로고의 정지 그림이 나와요.
- **보스전 상태 미리 만들기 불가**: iOS 앱은 자기 자신을 다른 프로세스로 실행할 수 없어서 BOSS FIGHT 목록은
  "준비 실패"로 표시돼요. 나머지 모드(아케이드, 랜덤 스폰, 서바이벌)는 그대로예요.
- **마우스 두 개로 2인 플레이, GUN TEST, 업데이트 확인 없음**. 2P는 게임패드 두 개로 할 수 있어요 (첫 번째 패드가 1P).
- 조준점 그림은 메뉴의 파일 선택 대신 Documents 폴더의 `crosshair.png`를 읽어요.

## 코드 구조

- `platform/compat/`: 게임 소스가 그대로 빌드되게 하는 작은 Win32 호환 레이어예요. 파일, 시간, 스레드, 키 상태,
  stb_truetype로 그리는 GDI 글자, SDL로 재생하는 PlaySound가 들어 있어요.
- `platform/vk_presenter_sdl.cpp`: `src/video/vk_presenter.cpp`를 SDL3 창과 Metal 레이어(MoltenVK)로 옮긴 거예요.
- `platform/touch_controls.*`: 터치 건이에요. 포인터(`GetCursorPos`)와 키(`VK_LBUTTON` 발사, `VK_RBUTTON` 재장전,
  `5`/`1` 코인/시작)로 동작해서, app.cpp의 마우스 처리(조준, 화면 밖 재장전, 프리 플레이)를 그대로 써요.
- `platform/`의 나머지는 Windows 전용 부분을 대신해요: SDL 오디오, minimp3 메뉴 음악, stb_image,
  Documents 폴더 연결.
- Linux에서도 `cmake -S . -B build/check -G Ninja`로 컴파일 확인용 빌드를 할 수 있어요 (Vulkan 드라이버와 SDL3 필요).
