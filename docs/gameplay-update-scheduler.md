# Gameplay update scheduler

## 등록과 수명

- `object::register_update()`와 `component::register_update()`는 등록된 객체에서 호출한다. 객체 또는 컴포넌트 해제 시 해당 owner의 update를 자동 제거한다.
- `update_scheduler::add()`를 직접 호출하는 경우 receiver의 lifetime과 update 제거는 호출자가 보장한다. owner와 receiver가 다르면 명시적으로 owner를 전달한다.
- owner는 `remove_all()`의 일괄 제거 기준이며, update handle에 대한 접근 권한을 제한하지 않는다.

## 조회와 변경

- `is_enabled()`는 현재 committed 상태를 조회한다. 다른 scheduler의 handle, 존재하지 않는 handle, 비활성 update는 `false`를 반환한다.
- `add()`, `remove()`, `remove_all()`, `set_enabled()`, `add_prerequisite()`, `remove_prerequisite()`는 최신 변경 상태를 대상으로 한다.
- `execute()` 중 변경은 해당 category 실행 종료 시 반영한다. 실행 중에는 현재 snapshot의 순서 및 활성 상태가 유지된다.
- 동일 category 안에서만 prerequisite을 설정할 수 있으며, 역방향 group 의존성, 순환 의존성 및 중복 의존성은 거절한다. 제거된 update의 prerequisite 참조도 함께 제거한다.
- 존재하지 않거나 다른 scheduler에서 발급한 handle에 대한 변경은 `false`를 반환한다. `remove_all()`은 제거된 update의 개수를 반환한다.
