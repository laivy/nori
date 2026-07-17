# nori CLI

`nori`는 `.nori` 리소스 파일을 조회, 수정, 삭제, 비교하는 명령줄 도구입니다.
리소스는 트리 형태로 저장되며, 파일 경로 뒤에 `/`로 하위 경로를 붙여 특정 값을 다룹니다.

## 빌드

프로젝트 프리셋은 `nori` CLI 빌드에 필요한 옵션을 켭니다.

```sh
cmake --preset windows-debug-x64
cmake --build --preset windows-debug-x64
```

Windows Debug 프리셋으로 빌드하면 실행 파일은 보통 다음 위치에 생성됩니다.

```text
build/windows-debug-x64/tools/nori/Debug/nori.exe
```

프리셋을 사용하지 않는 경우에는 최소한 다음 옵션을 켜야 합니다.

```sh
cmake -S . -B build/nori-cli -DNORI_BUILD_CORE=ON -DNORI_BUILD_RESOURCE=ON -DNORI_BUILD_CLI=ON
cmake --build build/nori-cli
```

## 경로 규칙

파일은 `.nori` 확장자를 사용합니다.
파일 안의 하위 리소스는 `.nori` 파일 경로 뒤에 `/`로 이어서 지정합니다.

```text
sample.nori
sample.nori/player
sample.nori/player/name
sample.nori/player/hp
```

## 사용법

```text
nori [-h | --help] <command> [<args>]
```

| 명령 | 설명 |
| --- | --- |
| `help` | 도움말을 출력합니다. |
| `tree <path>` | 지정한 파일 또는 하위 경로의 리소스 트리를 출력합니다. |
| `get <path>` | 지정한 경로의 값을 출력합니다. 문자열은 큰따옴표로 감싸서 출력됩니다. |
| `set <path> <value>` | 지정한 경로에 값을 저장하거나 갱신합니다. 파일이 없으면 생성합니다. |
| `del <path>` | 지정한 경로의 값을 삭제합니다. 하위 리소스도 함께 삭제됩니다. |
| `exists <path>` | 지정한 경로가 존재하면 `true`, 없으면 `false`를 출력합니다. |
| `diff <filepath1> <filepath2>` | 두 `.nori` 파일의 차이를 트리 형태로 출력합니다. |

## 값 입력 형식

`set` 명령의 `<value>`는 다음 형식을 지원합니다.

| 형식 | 예시 | 설명 |
| --- | --- | --- |
| 정수 | `100` | 범위에 따라 `Int32` 또는 `Int64`로 저장됩니다. |
| 실수 | `3.14` | `Float`로 저장됩니다. |
| 문자열 | `'hero'` | 작은따옴표 안의 내용이 문자열로 저장됩니다. |
| PNG 이미지 | `@image.png` | Windows에서만 지원합니다. PNG를 `r8g8b8a8` 이미지로 저장합니다. |

문자열에 공백이 있으면 사용하는 셸 규칙에 맞게 인자를 하나로 전달해야 합니다.

```sh
nori set sample.nori/player/title "'main hero'"
```

## 예시

리소스 파일을 만들고 값을 저장합니다.

```sh
nori set sample.nori/player/name 'hero'
nori set sample.nori/player/hp 100
nori set sample.nori/player/speed 3.5
```

값을 조회합니다.

```sh
nori get sample.nori/player/name
```

출력 예시는 다음과 같습니다.

```text
"hero"
```

트리 구조를 확인합니다.

```sh
nori tree sample.nori
```

경로 존재 여부를 확인합니다.

```sh
nori exists sample.nori/player/hp
```

값을 삭제합니다.

```sh
nori del sample.nori/player/hp
```

두 파일을 비교합니다.

```sh
nori diff before.nori after.nori
```

## 주의 사항

- `set`과 `del`은 경로 안에 `.nori` 파일명을 포함해야 합니다.
- `del sample.nori`처럼 파일 경로만 지정하면 `.nori` 파일 자체를 삭제합니다.
- `diff`는 `.nori` 확장자로 끝나는 파일 경로끼리만 비교합니다.
- 이미지 값 로딩은 Windows 전용이며, PNG 파일 경로 앞에 `@`를 붙여야 합니다.
