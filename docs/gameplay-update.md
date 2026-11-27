# Gameplay 업데이트

`object`와 `component`는 `on_register()`, `on_unregister()`로 수명을 관리하고, 필요한 업데이트 가상 함수만 재정의한다.

- `on_fixed_update(float delta_seconds)`: 고정 주기. 모든 호출이 완료되면 물리 입력 동기화, 물리 시뮬레이션, 물리 결과 동기화, 트리거 이벤트 전달 순으로 처리한다.
- `on_update(float delta_seconds)`: 가변 주기.
- `on_post_update(float delta_seconds)`: 모든 `on_update()` 호출이 끝난 후 같은 delta로 호출한다.

application은 누적 시간에 따라 fixed update를 0회 이상 실행한 후 variable update를 실행한다. 각 단계에서 world는 object의 내부 업데이트를 호출하고, object는 자신의 콜백과 소유 component의 콜백을 순서대로 호출한다. 서로 다른 object의 상대적 순서는 계약으로 보장하지 않는다.

각 `world::fixed_update()` 또는 `world::update()` 시작 시 사용자 콜백의 대상이 확정된다. 실행 중 등록된 object/component의 사용자 콜백은 다음 update 호출부터 참여할 수 있다. 물리 body는 등록 즉시 생성되므로 fixed update 중 새로 등록된 body도 그 step의 내부 물리 동기화 대상이다.

`world::create<T>()`로 object를 생성하고 `world::find(handle)`로 조회한다. `world::destroy(handle)`로 요청한 object 제거와 component 제거는 `end_frame()`까지 지연한다. component의 등록·해제 상태 및 콜백은 소유 object가 관리한다. 등록 콜백 시 component는 등록된 상태이며, 해제 콜백 시에는 등록 해제된 상태다. 제거 대기 중인 object/component도 해당 프레임에서 계속 업데이트된다. 업데이트 및 lifecycle 콜백 도중 재귀적 월드 실행, 프레임 종료, 즉시 월드 교체를 허용하지 않는다.

업데이트 등록, handle, priority, prerequisite, 개별 활성화 API는 제공하지 않는다. 단계 내부 실행 순서와 개별 로직의 활성 상태는 게임 코드가 관리한다. 물리 동기화는 `physics_body_component`가 담당하고 `world`가 호출 순서를 보장한다.
