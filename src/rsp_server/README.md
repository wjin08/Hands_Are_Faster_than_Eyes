# Raspberry Pi LED Control Server

## 개요
라즈베리파이에서 실행되는 LED 제어 서버 시스템입니다. TCP/IP 통신을 통해 여러 클라이언트로부터 LED 제어 명령을 받아 하드웨어를 제어하고, 모든 클라이언트에게 상태를 브로드캐스트합니다.


<img width="572" height="208" alt="스크린샷 2025-09-29 145847" src="https://github.com/user-attachments/assets/b59075f8-c0d1-4096-98d5-2294e681ff54" />

## 주요 기능
- **TCP 서버**: 포트 5000으로 다중 클라이언트 연결 지원
- **LED 하드웨어 제어**: 커널 모듈을 통한 GPIO LED 제어
- **브로드캐스트**: 한 클라이언트의 변경사항을 모든 클라이언트에게 실시간 전파
- **레벨 인디케이터 방식**: 0-255 값을 8단계 LED 패턴으로 변환
  - 0: 모든 LED OFF
  - 1-31: 1개 LED (O X X X X X X X)
  - 32-63: 2개 LED (O O X X X X X X)
  - 64-95: 3개 LED (O O O X X X X X)
  - 96-127: 4개 LED (O O O O X X X X)
  - 128-159: 5개 LED (O O O O O X X X)
  - 160-191: 6개 LED (O O O O O O X X)
  - 192-223: 7개 LED (O O O O O O O X)
  - 224-255: 8개 LED (O O O O O O O O)

## 빠른 시작 가이드

### 1. 사전 요구사항
```bash
# 필수 패키지 설치
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r) git
```

### 2. 소스코드 다운로드
```bash
git clone https://github.com/wjin08/Hands_Are_Faster_than_Eyes/tree/main/src/rsp_server.git
cd rsp_server
```

### 3. 컴파일 및 실행
```bash
# Makefile 실행 (커널 모듈 + 서버 프로그램 빌드)
make

# 커널 모듈 로드
sudo insmod ledkey_simple_dev.ko

# 디바이스 권한 설정
sudo chmod 666 /dev/ledkey

# 서버 실행
sudo ./ledkey_server
```

## 사용 방법

### 서버 실행 확인
```bash
# 서버 프로세스 확인
ps aux | grep ledkey_server

# 포트 리스닝 확인
netstat -tlnp | grep 5000

# 커널 모듈 로드 확인
lsmod | grep ledkey

# 디바이스 파일 확인
ls -la /dev/ledkey
```

### 로그 확인
서버 실행 시 콘솔에 다음과 같은 정보가 출력됩니다:
- 클라이언트 연결/해제
- LED 제어 명령 수신
- LED 패턴 변경 상태

## 문제 해결

### 커널 모듈 로드 실패
```bash
# 기존 모듈 제거
sudo rmmod ledkey_simple_dev

# 디바이스 파일 삭제
sudo rm /dev/ledkey

# dmesg 로그 확인
dmesg | tail -20

# 다시 로드
sudo insmod ledkey_simple_dev.ko
```

### 포트 사용 중 오류
```bash
# 5000번 포트 사용 프로세스 확인
sudo lsof -i :5000

# 프로세스 종료 후 재실행
sudo kill -9 [PID]
```

### GPIO 권한 오류
```bash
# GPIO 권한 확인
ls -la /sys/class/gpio/

# pi 사용자를 gpio 그룹에 추가
sudo usermod -a -G gpio pi
```

## 구조
```
rsp_server/                        # 라즈베리파이 서버
    ├── ledkey_server.c            # TCP 서버 프로그램
    ├── ledkey_simple_dev.c        # LED 제어 커널 모듈
    └── Makefile                   # 빌드 스크립트
```

## 주요 코드 설명

### LED 패턴 변환 (ledkey_server.c)
```c
// 다이얼 값(0-255)을 LED 개수로 변환
unsigned char value_to_led_pattern(unsigned char value) {
    int led_count;
    if (value <= 31) led_count = 1;
    else if (value <= 63) led_count = 2;
    // ... 8단계로 구분
    
    // 왼쪽(MSB)부터 LED 켜기
    for (int i = 0; i < led_count; i++) {
        pattern |= (1 << (7 - i));
    }
}
```

### 브로드캐스트 처리 (ledkey_server.c)
```c
// 모든 클라이언트에게 메시지 전송
void broadcast_to_all(char *message, int sender_fd) {
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(client_fds[i] != 0 && client_fds[i] != sender_fd) {
            write(client_fds[i], message, strlen(message));
        }
    }
}
```

### GPIO 제어 (ledkey_simple_dev.c)
```c
// GPIO LED 제어
for (i = 0; i < LED_COUNT; i++) {
    gpio_set_value(gpioLed[i], (kbuf & (0x01 << i)) ? 1 : 0);
}
```

## 네트워크 프로토콜
- **LED 제어 수신**: `[CLIENT_ID]LED@0xNN`
- **서버 브로드캐스트**: `[SERVER]LED_UPDATE@0xNN`
- **일반 메시지**: `[CLIENT_ID]메시지` 또는 `[ALLMSG]메시지`

## GPIO 핀 매핑
기본 설정 (ledkey_simple_dev.c):
```c
static int gpioLed[8] = {518, 519, 520, 521, 522, 523, 524, 525};
```
라즈베리파이 모델에 따라 GPIO 번호 수정이 필요할 수 있습니다.

---
