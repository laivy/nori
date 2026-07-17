# nori

[![build](https://github.com/laivy/nori/actions/workflows/build.yml/badge.svg)](https://github.com/laivy/nori/actions/workflows/build.yml)
[![release](https://img.shields.io/github/v/release/laivy/nori?display_name=tag)](https://github.com/laivy/nori/releases)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C.svg)](https://en.cppreference.com/w/cpp/23)

게임과 개발 도구에 필요한 공통 기능을 모듈 단위로 제공하는 C++23 정적 라이브러리입니다.
애플리케이션 실행과 이벤트 처리, 리소스 트리, DirectX 12 렌더링, TCP 네트워크를
각각 독립적인 CMake 타깃으로 사용할 수 있습니다.

## 주요 기능

- 애플리케이션 루프, 이벤트, 입력, 타이머와 스레드 풀
- 계층형 리소스 저장소와 `.nori` 직렬화
- DirectX 12 렌더링과 선택적 Dear ImGui 연동
- 비동기 TCP 클라이언트와 서버
- 모듈별 CMake 타깃과 설치 가능한 CMake 패키지
- Windows와 Linux CI 빌드

## 빠른 시작

### FetchContent

다른 CMake 프로젝트에서 저장소를 직접 가져와 필요한 모듈을 링크할 수 있습니다.
재현 가능한 빌드를 위해 `GIT_TAG`에는 태그나 커밋 해시를 지정하세요.

```cmake
include(FetchContent)

set(NORI_BUILD_CORE ON)
set(NORI_BUILD_RESOURCE ON)
set(NORI_BUILD_GRAPHICS ON)

FetchContent_Declare(
    nori
    GIT_REPOSITORY https://github.com/laivy/nori.git
    GIT_TAG <tag-or-commit>
)
FetchContent_MakeAvailable(nori)

target_link_libraries(my_game PRIVATE
    nori::core
    nori::resource
    nori::graphics
)
```

### 사전 빌드 SDK

릴리스에서 받은 SDK의 압축을 해제하고 `CMAKE_PREFIX_PATH`에 경로를 지정하면
nori와 빌드 의존성을 다시 컴파일하지 않고 사용할 수 있습니다.

```cmake
find_package(nori CONFIG REQUIRED COMPONENTS core resource graphics network)

target_link_libraries(my_game PRIVATE
    nori::core
    nori::resource
    nori::graphics
    nori::network
)
```

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=<path-to-extracted-nori>
cmake --build build --config Debug
```

SDK 파일 이름은 플랫폼과 컴파일러에 따라 다음 형식으로 생성됩니다.

```text
nori-<version>-<platform>-<compiler>-<architecture>.zip
```

Windows SDK는 Debug와 Release 정적 라이브러리를 모두 포함합니다.

## 모듈

| CMake 타깃 | 기능 | 내부 의존성 | 플랫폼 |
| --- | --- | --- | --- |
| `nori::core` | 애플리케이션, 이벤트, 입력, 타이머, 스레드 풀 | 없음 | Windows, Linux¹ |
| `nori::resource` | 리소스 트리, 경로, 핸들, 직렬화 | `nori::core` | Windows, Linux |
| `nori::graphics` | DirectX 12 렌더링, 선택적 Dear ImGui 연동 | `nori::core`, `nori::resource` | Windows |
| `nori::network` | TCP 클라이언트, 서버와 패킷 직렬화 | 없음 | Windows, Linux |

¹ `nori::core`의 Win32 애플리케이션 API는 Windows에서만 제공됩니다.

### `nori::core`

- 공개 헤더: `include/nori/core.hpp`, `include/nori/core/*`
- `application`, `console_application`, 이벤트 디스패처와 입력 코드
- 타이머, 스레드 풀, 슬롯 맵과 범용 유틸리티
- Windows에서 `windows_application`과 Win32 메시지 기반 애플리케이션 제공
- 외부 의존성 없음

### `nori::resource`

- 공개 헤더: `include/nori/resource.hpp`, `include/nori/resource/*`
- 리소스 `handle`, 경로 규칙과 계층형 부모/자식 관계
- 정수, 실수, 문자열과 이미지 값을 지원하는 `.nori` 직렬화
- `nori::core`에 의존
- 외부 의존성 없음

경로 조회 실패는 빈 `handle` 또는 `std::optional`로 표현하고, 이름 충돌이나 잘못된
관계는 `resource::result<T>`로 반환합니다. 유효하지 않은 핸들과 타입 계약 위반은
프로그래밍 오류로 취급합니다.

### `nori::graphics`

- 공개 헤더: `include/nori/graphics.hpp`, `include/nori/graphics/*`
- `graphics::draw_list`로 제출한 명령을 DirectX 12 백엔드로 렌더링
- `nori::core`, `nori::resource`에 의존
- DirectX-Headers와 Windows `d3d12`, `dxgi` 사용
- `NORI_ENABLE_IMGUI=ON`에서 Dear ImGui의 DX12/Win32 백엔드 제공
- Windows 전용

### `nori::imgui`

- `NORI_ENABLE_IMGUI=ON`으로 생성한 패키지에서 제공
- nori가 사용하는 Dear ImGui 헤더, 컴파일 설정 및 구현을 함께 제공
- `nori::graphics`를 전이적으로 링크하므로 별도의 Dear ImGui 설치가 필요하지 않음
- 다른 Dear ImGui 라이브러리와 함께 링크하는 구성은 지원하지 않음

설치된 패키지에서 Dear ImGui API를 직접 사용하는 경우 다음과 같이 연결합니다.

```cmake
find_package(nori REQUIRED COMPONENTS imgui)
target_link_libraries(my_app PRIVATE nori::imgui)
```

이후 `<imgui.h>`, `<imgui_internal.h>`, `<backends/imgui_impl_win32.h>` 또는
`<misc/cpp/imgui_stdlib.h>`를 별도의 include 경로 설정 없이 사용할 수 있습니다.

### `nori::network`

- 공개 헤더: `include/nori/network.hpp`, `include/nori/network/*`
- 비동기 TCP 연결, 연결/해제 핸들러와 패킷 대기
- `in_packet`, `out_packet`을 통한 정수, 실수, 문자열 직렬화
- Boost.Asio와 `Threads::Threads` 사용
- TCP 전용이며 UDP는 지원하지 않음

## 요구 사항

- CMake 4.0 이상
- C++23 지원 컴파일러
- Windows: MSVC와 Windows SDK가 포함된 C++ 개발 환경
- Linux: C++23 지원 GCC 또는 호환 컴파일러와 Ninja
- 첫 구성 시 외부 의존성을 내려받을 수 있는 네트워크 연결

외부 의존성은 CMake `FetchContent`로 가져옵니다. 선택한 모듈에 따라 Boost.Asio,
DirectX-Headers, Dear ImGui 또는 GoogleTest가 사용됩니다.

## 소스에서 빌드

저장소의 CMake Workflow Preset은 nori 개발과 검증을 위한 설정입니다.
빌드 결과는 `build/<preset>` 아래에 생성됩니다.

Windows Debug 빌드:

```sh
cmake --workflow --preset dev
```

Windows Release 빌드와 테스트:

```sh
cmake --workflow --preset verify
```

Linux Release 빌드와 테스트:

```sh
cmake --workflow --preset linux-verify
```

Windows 또는 Linux SDK 패키지 생성:

```sh
cmake --workflow --preset package
cmake --workflow --preset linux-package
```

## CMake 옵션

모든 옵션의 기본값은 `OFF`입니다. 단, 최상위 프로젝트에서는 `NORI_INSTALL`이
기본적으로 활성화됩니다. 제공되는 preset은 필요한 옵션을 자동으로 설정합니다.

| 옵션 | 설명 |
| --- | --- |
| `NORI_BUILD_CORE` | `nori::core` 빌드 |
| `NORI_BUILD_RESOURCE` | `nori::resource` 빌드; `NORI_BUILD_CORE` 필요 |
| `NORI_BUILD_GRAPHICS` | `nori::graphics` 빌드; core, resource와 Windows 필요 |
| `NORI_BUILD_NETWORK` | `nori::network` 빌드 |
| `NORI_ENABLE_IMGUI` | graphics의 Dear ImGui 연동 활성화 |
| `NORI_BUILD_CLI` | `nori` 명령줄 도구 빌드; core와 resource 필요 |
| `NORI_BUILD_NORITOR` | Windows용 `noritor` 리소스 편집기 빌드; graphics와 ImGui 필요 |
| `BUILD_TESTING` | 최상위 프로젝트에서 테스트 빌드 |
| `NORI_INSTALL` | 라이브러리와 CMake 패키지 메타데이터 설치 |

직접 옵션을 지정하는 경우 의존 모듈도 함께 활성화해야 합니다.

```sh
cmake -S . -B build/custom \
    -DNORI_BUILD_CORE=ON \
    -DNORI_BUILD_RESOURCE=ON
cmake --build build/custom
```

설치는 표준 CMake 명령을 사용합니다.

```sh
cmake --install build/custom
```

## 도구

### `nori`

`NORI_BUILD_CLI=ON`에서 빌드되는 명령줄 도구입니다. `nori::core`와
`nori::resource`를 사용하며 Windows에서는 `windowscodecs`에도 의존합니다.

### `noritor`

리소스를 시각적으로 편집하는 Windows GUI 도구입니다. `NORI_BUILD_NORITOR=ON`과
`NORI_ENABLE_IMGUI=ON`이 필요하며 `nori::graphics`를 사용합니다.

## 라이선스

nori는 MIT License로 배포됩니다. 패키지에 포함된 nori와 서드파티 의존성의
라이선스 전문은 `licenses/` 디렉터리에서 확인할 수 있습니다.

## 테스트

테스트는 nori가 최상위 프로젝트이고 `BUILD_TESTING=ON`일 때 추가됩니다.
GoogleTest는 `FetchContent`로 준비됩니다.

```sh
cmake -S . -B build/tests \
    -DNORI_BUILD_CORE=ON \
    -DNORI_BUILD_RESOURCE=ON \
    -DBUILD_TESTING=ON
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure
```
