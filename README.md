# nori

[![build](https://github.com/laivy/nori/actions/workflows/build.yml/badge.svg)](https://github.com/laivy/nori/actions/workflows/build.yml)
[![release](https://img.shields.io/github/v/release/laivy/nori?display_name=tag)](https://github.com/laivy/nori/releases)

게임 개발에 필요한 정적 라이브러리 모음

## 모듈

| 모듈       | 기능                                                                         |
|------------|------------------------------------------------------------------------------|
| `core`     | 애플리케이션, 이벤트, 입력, 타이머, 스레드 풀, 벡터와 행렬 타입 및 기본 연산 |
| `io`       | coroutine 실행, TCP 통신과 패킷 직렬화                                       |
| `resource` | 리소스 트리, 경로, 핸들, 직렬화                                              |
| `asset`    | 이미지, 모델, 셰이더와 바이너리 에셋 cooking                                 |
| `graphics` | DirectX 12 렌더링, 선택적 Dear ImGui 연동                                    |

## 요구 사항

- CMake 4.2 이상
- C++23

| 플랫폼  | 운영체제             | 컴파일러 |
|---------|----------------------|----------|
| Windows | Windows 10 1803 이상 | MSVC     |
| Linux   | Linux                | GCC      |

## 라이선스

[MIT License](LICENSE)
