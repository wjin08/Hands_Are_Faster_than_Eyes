# LED Control QT Server Manager

## 개요
라즈베리파이의 LED를 원격으로 제어하고 모니터링 하는 Qt 기반 클라이언트-서버 시스템입니다. 여러 클라이언트가 동시에 접속하여 실시간으로 LED 상태를 공유하고 제어할 수 있습니다.



## 주요 기능
- **실시간 LED 제어**: 다이얼을 돌려 0-255 값으로 LED 개수 조절
- **다중 클라이언트 지원**: 여러 사용자가 동시 접속 가능
- **실시간 동기화**: 한 클라이언트의 변경사항이 모든 클라이언트에 즉시 반영
- **채팅 기능**: 클라이언트 간 메시지 송수신


## 빠른 시작 가이드

### 1. 사전 요구사항

#### Qt 클라이언트 (Ubuntu/Linux)
```bash
# Qt5 개발 환경 설치
sudo apt update
sudo apt install qt5-default qtcreator build-essential
```

### 2. 소스코드 다운로드
```bash
git clone https://github.com/wjin08/Hands_Are_Faster_than_Eyes/edit/main/src/qt.git
cd led-remote-control
```

### 3. Qt 클라이언트 빌드 및 실행

#### 방법 1: Qt Creator 사용
1. Qt Creator 실행
2. File → Open File or Project
3. `AiotClient.pro` 파일 선택
4. Build → Run (Ctrl+R)

#### 방법 2: 터미널 사용
```bash
cd qt-client
qmake AiotClient.pro
make
./AiotClient
```


## 사용 방법

### Qt 클라이언트 사용
1. **서버 연결**
   - Tab2 (Socket Client) 선택
   - "서버 연결" 버튼 클릭
   - 라즈베리파이 IP 주소 입력 (예: 192.168.0.204)

2. **LED 제어**
   - Tab1 (Device Control) 선택
   - 다이얼을 돌려 LED 제어
   - 또는 타이머를 설정하여 자동 제어

3. **메시지 송신**
   - Tab2에서 메시지 입력 후 "송신" 클릭
   - 모든 클라이언트가 메시지 수신

## 문제 해결


### Qt 빌드 오류
```bash
# Qt 버전 확인
qmake --version

# 필요 라이브러리 설치
sudo apt install libqt5network5 libqt5widgets5
```

## 구조
```
qt-client/                     # Qt 클라이언트
    ├── AiotClient.pro             # Qt 프로젝트 파일
    ├── main.cpp                   # 메인 함수
    ├── mainwidget/                # 메인 위젯 관련
    │   ├── mainwidget.cpp
    │   └── mainwidget.h
    ├── tab1_devcontrol/           # LED 제어 탭
    │   ├── tab1devcontrol.cpp
    │   └── tab1devcontrol.h
    ├── tab2_socketclient/         # 소켓 통신 탭
    │   ├── tab2socketclient.cpp
    │   └── tab2socketclient.h
    └── socketclient/              # 소켓 클라이언트 모듈
        ├── socketclient.cpp
        └── socketclient.h

```

## 주요 코드 설명

### 실시간 동기화 (클라이언트)
```cpp
// 다른 클라이언트의 변경사항 수신 시
void Tab1DevControl::updateLedFromServer(int value) {
    isUpdatingFromServer = true;  // 무한루프 방지
    ui->pDialLed->setValue(value);
    isUpdatingFromServer = false;
}
```

---
