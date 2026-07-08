# 터보펌프 계측 제어 GUI 실습 (Qt6/C++ + Serial)

원본 WPF 프로젝트(`WPFTurboPumpDemo`)를 **Qt6 / C++ / CMake** 로 포팅한 버전.
`SerialGuiApp`(Qt Widgets GUI 클라이언트)와 `DeviceSimulator`(가상 장비 역할을
하는 콘솔 프로그램) 두 개로 구성되며, 통신 프로토콜과 UI/기능은 원본과 동일함.

```
[SerialGuiApp (Qt Widgets)]  <-- 가상 COM 포트 페어 -->  [DeviceSimulator (콘솔)]
```

이 저장소는 컨테이너 환경(Ubuntu 24.04 + Qt 6.4.2)에서 **실제로 빌드/실행까지
검증**되었음 (경고 0건, GUI 정상 기동, 시뮬레이터 오류 처리 정상 동작).

## 1. 필요 패키지

Ubuntu/Debian 계열:
```bash
sudo apt-get install qt6-base-dev qt6-serialport-dev qt6-charts-dev cmake build-essential
```

Windows에서는 Qt 공식 온라인 인스톨러로 "Qt 6.x" + "Qt SerialPort" + "Qt Charts"
컴포넌트를 설치하고, Visual Studio 2022 의 "Desktop development with C++" 워크로드
또는 Qt Creator를 사용하면 됨.

## 2. 빌드

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
```

빌드 결과물:
- `build/SerialGuiApp/SerialGuiApp` — GUI 클라이언트
- `build/DeviceSimulator/DeviceSimulator` — 가상 장비 시뮬레이터

## 3. 가상 COM 포트 페어 만들기

원본 README와 동일. Windows에서는 `freevirtualserialports.com` (권장) 또는
com0com을 사용. Linux에서 테스트하려면 `socat` 으로 가상 페어를 만들 수 있음:

```bash
socat -d -d pty,raw,echo=0,link=/tmp/ttyGUI pty,raw,echo=0,link=/tmp/ttySIM
```

## 4. 실행 순서

1. **DeviceSimulator 먼저 실행**
   ```bash
   ./DeviceSimulator/DeviceSimulator /tmp/ttySIM 9600
   ```
   포트를 인자로 안 주면 실행 중에 직접 입력받음.

2. **SerialGuiApp 실행**
   ```bash
   ./SerialGuiApp/SerialGuiApp
   ```
   포트 콤보박스에서 나머지 포트(`/tmp/ttyGUI` 등)를 선택하고 **연결** 클릭.

## 5. 통신 프로토콜 요약 (원본과 동일)

| 방향 | 메시지 | 의미 |
|---|---|---|
| GUI → 장비 | `GET:TEMP` | 온도 조회 요청 |
| GUI → 장비 | `GET:PRESSURE` | 압력 조회 요청 |
| GUI → 장비 | `SET:VALVE:OPEN` / `SET:VALVE:CLOSE` | 밸브 개폐 명령 |
| 장비 → GUI | `TEMP:23.45` | 온도 조회 응답 |
| 장비 → GUI | `PRESSURE:101.32` | 압력 조회 응답 |
| 장비 → GUI | `ACK:VALVE:OPEN` | 밸브 명령 확인 응답 |
| 장비 → GUI | `TELEMETRY:TEMP:..,PRESSURE:..,VALVE:..` | 2초 주기 비동기 상태 보고 |

## 6. WPF → Qt 포팅 시 핵심 차이점

- **스레딩 모델**: .NET의 `SerialPort.DataReceived`는 백그라운드 스레드에서
  발생하므로 `Dispatcher.Invoke`로 UI 스레드에 넘겨야 했음. Qt의 `QSerialPort`는
  이벤트 루프 기반 비동기 I/O라서 `readyRead` 시그널이 GUI 스레드에서 그대로
  발생함 — 별도의 스레드 전환 코드가 필요 없음 (`mainwindow.cpp`,
  `devicesimulator.cpp` 상단 주석 참고).
- **UI 선언 방식**: WPF는 XAML(선언적 마크업) + code-behind 조합이지만, 이
  포팅판은 `mainwindow.cpp`의 `setupUi()`에서 위젯/레이아웃을 코드로 직접
  구성함 (Qt Designer의 `.ui` 파일 대신 프로그래밍 방식 UI).
- **그래프**: OxyPlot → **Qt Charts** (`QChart`, `QLineSeries`, `QValueAxis`).
  이중 Y축(온도/압력) 구성과 최대 300포인트 유지, 자동 스케일 로직을 동일하게
  구현함.
- **DispatcherTimer** → **QTimer** (재연결 타이머, 하트비트 타이머 모두 동일한
  간격/타임아웃 값 유지: 재연결 3초, 하트비트 타임아웃 6초).
- **DeviceSimulator의 동시성**: .NET 버전은 `System.Threading.Timer`(별도
  스레드)와 `DataReceived`(백그라운드 스레드)가 동시에 상태를 건드릴 수 있어
  `lock`이 필요했음. Qt 버전은 `QTimer`와 `readyRead`가 모두 같은 이벤트 루프
  스레드에서 순차 실행되므로 별도의 mutex가 필요 없음.

## 7. 확장 아이디어

- 통신 로그를 파일로 저장하는 기능은 이미 포함(원본과 동일: 수동 저장 + 자동
  실시간 저장).
- 재연결 로직도 이미 포함(원본과 동일: 자동 재연결 체크박스 + 하트비트 감지).
- 프로토콜을 텍스트 라인 대신 JSON으로 바꿔보기.
- Qt Designer `.ui` 파일 기반으로 리팩터링해보며 코드형 UI와의 차이 비교.
- QML/Quick 버전으로 다시 만들어 QtWidgets와의 차이 비교.
