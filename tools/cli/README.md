# nori CLI

AI가 `.nori` resource를 조회·편집·변환할 때 사용하는 전용 CLI 도구입니다.
한 번의 프로세스 실행에서 표준 입력의 UTF-8 JSON 객체 하나를 EOF까지 읽고, 표준 출력으로 JSON 응답 하나를 반환합니다.

## 요청

```json
{
  "method": "get",
  "params": {
    "path": "sample.nori::player/name"
  }
}
```

`params`는 object만 허용합니다. 각 method에 정의되지 않은 `params` 필드는 `invalid_argument`로 처리합니다.

## 응답

성공:

```json
{
  "result": {
    "type": "string",
    "value": "hero"
  }
}
```

출력이 없는 명령의 성공:

```json
{
  "result": null
}
```

실패:

```json
{
  "error": {
    "code": "not_found"
  }
}
```

성공은 종료 코드 `0`, 처리된 실패는 non-zero를 사용합니다. 처리된 실패의 JSON도 표준 출력에 기록합니다. 표준 오류는 치명적 비프로토콜 진단에만 사용합니다.
응답 JSON object의 property 순서는 method별 schema에 고정합니다. resource 표현은 `type`을 먼저 출력하고 `value` 또는 metadata를 뒤에 출력하며, `tree` node는 `name`, resource 표현, `children` 또는 `truncated` 순서를 사용합니다. `children` 배열 자체는 resource의 sibling 순서를 유지합니다.
CLI는 JSON 구조와 필드 형식만 검증하고 resource의 경로·관계·충돌 유효성은 별도로 재검증하지 않습니다. resource 관련 실패는 Nori 모듈의 오류를 CLI 프로토콜 오류 코드로 변환합니다.

## Method

| method | params |
| --- | --- |
| `tree` | `path`, `depth?` |
| `get` | `path` |
| `exists` | `path` |
| `diff` | `before`, `after` |
| `set` | `path`, `type`, `value?` |
| `import` | `type`, `path`, `source`, `format?`, `quality?` |
| `copy` | `source`, `destination` |
| `move` | `source`, `destination` |
| `rename` | `path`, `name` |
| `reorder` | `path`, `position` |
| `del` | `path` |

## 데이터 타입

`set`은 `empty`, `bool`, `int32`, `int64`, `float`, `string`을 지원합니다. `empty`는 `value`를 사용하지 않습니다.
`image`, `binary`는 `import`로 생성합니다.

`get`에서 `image`는 `format`, `width`, `height`, `mip_levels`, `binary`는 `size` metadata만 반환합니다.

## tree

`depth: 0`은 target의 child를 확장하지 않고, `depth: 1`은 직접 child까지 확장합니다. `depth`를 생략하면 깊이 제한이 없습니다. 실제 child가 있지만 depth 제한으로 생략된 node에는 `truncated: true`가 포함됩니다. `.nori` document를 조회하면 document 자체 표현 없이 `children`을 반환하고, 특정 resource를 조회하면 해당 resource 자신의 `name`, resource 표현과 `children` 또는 `truncated`를 포함한 node 전체를 반환합니다. 출력되는 `children` 배열은 resource의 sibling 순서를 유지합니다.

## diff

두 resource의 상대 순서를 유지한 child는 제외하고 순서를 깨뜨린 child만 `reorder`로 보고합니다. 단일 이동은 항목 하나만 출력됩니다.

## import

`type`은 `binary`, `image`, `model`, `shader`를 지원합니다.

- `image.format`: `rgba8`, `bc5`, `bc7`, `bc7-srgb`, 기본값 `rgba8`
- `image/model.quality`: `quick`, `normal`, 기본값 `quick`
- `format`은 image에서만 사용
- `quality`는 image와 model에서만 사용

## 오류 코드

- `invalid_request`
- `unknown_method`
- `invalid_argument`
- `not_found`
- `already_exists`
- `type_mismatch`
- `io_error`
- `invalid_format`
- `unsupported_format`
- `unsupported_platform`
- `internal_error`
