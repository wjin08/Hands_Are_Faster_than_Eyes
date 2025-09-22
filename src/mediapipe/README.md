# mediapipe를 이용한 손 인식

## 1. 필수 패키치
### 1) 패키지 설치
```bash
$ sudo apt update
$ sudo apt install -y build-essential git cmake python3 python3-pip \
                    libopencv-dev clang git-lfs

# Bazelisk 설치
$ sudo wget -O /usr/local/bin/bazel https://github.com/bazelbuild/bazelisk/releases/download/v1.19.0/bazelisk-linux-amd64
$ sudo chmod +x /usr/local/bin/bazel

# MediaPipe 가져오기
$ git clone https://github.com/google-ai-edge/mediapipe.git
$ cd mediapipe

# 디렉토리 존재 보장
$ mkdir -p mediapipe/modules/palm_detection
$ mkdir -p mediapipe/modules/hand_landmark

# palm detection / hand landmark full 모델 다운로드
$ wget -O mediapipe/modules/palm_detection/palm_detection_full.tflite \
  https://storage.googleapis.com/mediapipe-assets/palm_detection_full.tflite

$ wget -O mediapipe/modules/hand_landmark/hand_landmark_full.tflite \
  https://storage.googleapis.com/mediapipe-assets/hand_landmark_full.tflite

```

### 2) 폴더 구조 만들기
```bash
$ cd mediapipe

$ mkdir -p examples/custom/hand_palm_demo

$ cd ../
```

---

## 2. 코드 작성

### 1) `hand_tracker.cpp`코드 생성
```bash
$ vi mediapipe/examples/custom/hand_palm_demo/hand_tracker.cpp
```

### 2) BUILD 파일 생성
```bash
$ vi mediapipe/examples/custom/hand_palm_demo/BUILD
```

### 3) exports_files 섹션 수정
```bash
# palm_detection
$ printf '\nexports_files(["palm_detection_full.tflite"])\n' \
  >> mediapipe/modules/palm_detection/BUILD

# hand_landmark
$ printf '\nexports_files([\n    "hand_landmark_full.tflite",\n    "handedness.txt",\n])\n' \
  >> mediapipe/modules/hand_landmark/BUILD
```

---

## 3. 빌드 및 실행

### 1) 빌드
```bash
# clang 방식
$ bazel build -c opt --define MEDIAPIPE_DISABLE_GPU=1   --action_env=CC=/usr/bin/clang --action_env=CXX=/usr/bin/clang++   --repo_env=CC=/usr/bin/clang --repo_env=CXX=/usr/bin/clang++   --repo_env=HERMETIC_PYTHON_VERSION=3.12   --define xnn_enable_avxvnniint8=false --define xnn_enable_avx512fp16=false   --copt=-I/usr/include/opencv4 --cxxopt='-std=gnu++20' --host_cxxopt='-std=gnu++20'   //mediapipe/examples/custom/hand_palm_demo:hand_palm_demo
```
빌드 클린
```bash
# 소프트 클린
$ bazel clean

# 하드 클린
$ bazel shutdown
$ bazel clean --expunge
```

### 2) 실행
```bash
$ ./bazel-bin/mediapipe/examples/custom/hand_palm_demo/hand_palm_demo \
  mediapipe/graphs/hand_tracking/hand_tracking_desktop_live.pbtxt
```