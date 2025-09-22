// hand_x255_view.cc
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <cstdlib>    // getenv, atoi
#include <mutex>      // std::mutex
#include <sstream>    // std::ostringstream
#include <cmath>      // std::lround

#include "mediapipe/framework/calculator_graph.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/framework/port/logging.h"
#include "mediapipe/framework/port/ret_check.h"
#include "mediapipe/framework/port/file_helpers.h"

#include <google/protobuf/text_format.h>
#include <opencv2/opencv.hpp>

using ::mediapipe::CalculatorGraph;
using ::mediapipe::ImageFrame;
using ::mediapipe::Packet;
using ::mediapipe::NormalizedLandmarkList;

// BGR(cv::Mat) -> MediaPipe ImageFrame(SRGB)
static std::unique_ptr<ImageFrame> MatToImageFrameRGB(const cv::Mat& bgr) {
  auto frame = std::make_unique<ImageFrame>(
      mediapipe::ImageFormat::SRGB, bgr.cols, bgr.rows,
      ImageFrame::kDefaultAlignmentBoundary);
  cv::Mat dst(frame->Height(), frame->Width(), CV_8UC3, frame->MutablePixelData());
  cv::cvtColor(bgr, dst, cv::COLOR_BGR2RGB);
  return frame;
}

// 손바닥 중심(간단 평균) 픽셀 좌표
static cv::Point2f ComputePalmCenterPx(const NormalizedLandmarkList& lm, int w, int h) {
  static const int idxs[] = {0, 1, 5, 9, 13, 17};
  float sx = 0.f, sy = 0.f; int cnt = 0;
  for (int i : idxs) if (i < lm.landmark_size()) {
    const auto& p = lm.landmark(i);
    sx += p.x() * w;
    sy += p.y() * h;
    ++cnt;
  }
  if (cnt == 0) return {-1.f, -1.f};
  return {sx / cnt, sy / cnt};
}

struct CamTry { int index; int backend; int fourcc; const char* name; };

static bool TryOpen(cv::VideoCapture& cap, const CamTry& t, int w, int h, int fps) {
  std::cerr << "[CAM] try backend=" << t.name << " idx=" << t.index << "\n";
  bool ok = (t.backend == cv::CAP_ANY) ? cap.open(t.index) : cap.open(t.index, t.backend);
  if (!ok) { std::cerr << "[CAM] open fail\n"; return false; }
  if (w>0) cap.set(cv::CAP_PROP_FRAME_WIDTH,  w);
  if (h>0) cap.set(cv::CAP_PROP_FRAME_HEIGHT, h);
  if (fps>0) cap.set(cv::CAP_PROP_FPS, fps);
  cap.set(cv::CAP_PROP_BUFFERSIZE, 1);
  if (t.fourcc) cap.set(cv::CAP_PROP_FOURCC, t.fourcc); // MJPG (미지원이면 무시)
  cv::Mat test;
  if (!cap.read(test) || test.empty()) {
    std::cerr << "[CAM] read test frame FAIL\n";
    cap.release(); return false;
  }
  std::cerr << "[CAM] OK: " << test.cols << "x" << test.rows << "\n";
  return true;
}

static bool OpenCameraAuto(cv::VideoCapture& cap, int& out_w, int& out_h,
                           int target_w=640, int target_h=480, int fps=30) {
  const CamTry tries[] = {
    {0, cv::CAP_V4L2, cv::VideoWriter::fourcc('M','J','P','G'), "V4L2+MJPG"},
    {0, cv::CAP_V4L2, 0,                                        "V4L2+RAW"},
    {0, cv::CAP_ANY,  cv::VideoWriter::fourcc('M','J','P','G'), "ANY+MJPG"},
    {0, cv::CAP_ANY,  0,                                        "ANY+RAW"},
  };
  for (auto& t : tries) {
    if (TryOpen(cap, t, target_w, target_h, fps)) {
      out_w = (int)cap.get(cv::CAP_PROP_FRAME_WIDTH);
      out_h = (int)cap.get(cv::CAP_PROP_FRAME_HEIGHT);
      std::cerr << "[CAM] final size " << out_w << "x" << out_h << ", fps req=" << fps << "\n";
      return true;
    }
  }
  return false;
}

// === 0~255 맵핑 ===
static int MapXTo255(float x_px, int width) {
  if (width <= 1) return 0;
  float v = (x_px / (float)(width - 1)) * 255.0f;
  if (v < 0.f) v = 0.f;
  if (v > 255.f) v = 255.f;
  return (int)std::lround(v);
}

// === 스케일 바 ===
static void DrawScaleBar(cv::Mat& img, int value255) {
  const int margin = 10;
  const int bar_h = std::max(24, img.rows / 20);
  int y0 = img.rows - bar_h - margin;

  // 바 배경
  cv::rectangle(img, {margin, y0}, {img.cols - margin, y0 + bar_h}, cv::Scalar(30,30,30), cv::FILLED);

  // 눈금 (0,64,128,192,255)
  const int ticks[] = {0,64,128,192,255};
  for (int t : ticks) {
    int x = margin + (img.cols - 2*margin) * t / 255;
    cv::line(img, {x, y0}, {x, y0 + bar_h}, cv::Scalar(80,80,80), 1);
    char buf[8]; snprintf(buf, sizeof(buf), "%d", t);
    cv::putText(img, buf, {x+2, y0 - 4}, cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(200,200,200), 1);
  }

  // 현재 값 커서
  int xv = margin + (img.cols - 2*margin) * value255 / 255;
  cv::line(img, {xv, y0 - 6}, {xv, y0 + bar_h + 6}, cv::Scalar(0,255,255), 2);

  // 라벨
  char label[32]; snprintf(label, sizeof(label), "X255:%d", value255);
  cv::putText(img, label, {margin, y0 - 10}, cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0,255,255), 2);
}

// === FPS 미터 ===
struct FpsMeter {
  int frames = 0;
  std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
  double fps = 0.0;
  double update() {
    using clock = std::chrono::steady_clock;
    frames++;
    auto t1 = clock::now();
    double dt = std::chrono::duration<double>(t1 - t0).count();
    if (dt >= 0.25) {
      fps = frames / dt;
      frames = 0; t0 = t1;
    }
    return fps;
  }
};

// === 테두리 프레임 + 캡션 ===
static void DrawDisplayFrame(cv::Mat& img, int x255, double fps,
                             const std::string& title = "Hand -> X(0~255)") {
  double scale = std::max(1.0, img.cols / 640.0);
  int border = int(std::round(6 * scale));
  int pad    = int(std::round(10 * scale));
  double font = 0.6 * scale;
  int thick = std::max(1, int(std::round(2 * scale)));

  // 바깥 테두리
  cv::rectangle(img, {0,0}, {img.cols-1, img.rows-1}, cv::Scalar(0,0,0), border);
  // 안쪽 밝은 테두리
  cv::rectangle(img, {border, border}, {img.cols-1-border, img.rows-1-border}, cv::Scalar(200,200,200), 1);

  // 캡션 박스
  int box_w = int(std::round(300 * scale));
  int box_h = int(std::round(40 * scale));
  cv::Rect cap(border + pad, border + pad, box_w, box_h);
  cv::rectangle(img, cap, cv::Scalar(25,25,25), cv::FILLED);
  cv::rectangle(img, cap, cv::Scalar(80,80,80), 1);

  // 텍스트
  char line1[128], line2[128];
  snprintf(line1, sizeof(line1), "%s", title.c_str());
  if (x255 >= 0) snprintf(line2, sizeof(line2), "X255=%d   FPS=%.1f", x255, fps);
  else           snprintf(line2, sizeof(line2), "X255=-    FPS=%.1f", fps);

  int base = 0;
  cv::Size sz1 = cv::getTextSize(line1, cv::FONT_HERSHEY_SIMPLEX, font, thick, &base);
  cv::Size sz2 = cv::getTextSize(line2, cv::FONT_HERSHEY_SIMPLEX, font, thick, &base);

  int tx = cap.x + pad;
  int ty = cap.y + pad + sz1.height;
  cv::putText(img, line1, {tx, ty}, cv::FONT_HERSHEY_SIMPLEX, font, cv::Scalar(0,255,255), thick);
  cv::putText(img, line2, {tx, ty + sz2.height + int(6*scale)}, cv::FONT_HERSHEY_SIMPLEX, font, cv::Scalar(200,200,200), thick);
}

int main(int argc, char** argv) {
  // 그래프 경로
  std::string graph_path = "mediapipe/graphs/hand_tracking/hand_tracking_desktop_live.pbtxt";
  if (argc > 1 && argv[1][0] != '-') graph_path = argv[1];

  // 해상도/프레임레이트 옵션
  int req_w = 640, req_h = 480, req_fps = 30;
  for (int i=1;i<argc;i++) {
    std::string a = argv[i];
    if (a == "--res" && i+1 < argc) {
      std::string r = argv[++i];
      auto x = r.find('x');
      if (x != std::string::npos) {
        req_w = std::atoi(r.substr(0,x).c_str());
        req_h = std::atoi(r.substr(x+1).c_str());
      }
    } else if (a == "--fps" && i+1 < argc) {
      req_fps = std::atoi(argv[++i]);
    }
  }

  // GUI 가능 여부
  bool gui_ok = (std::getenv("DISPLAY") || std::getenv("WAYLAND_DISPLAY"));
  for (int i=1;i<argc;i++) {
    if (std::string(argv[i]) == "--no-gui") gui_ok = false;
    if (std::string(argv[i]) == "--gui")    gui_ok = true;
  }
  std::cerr << "[APP] GUI " << (gui_ok ? "ENABLED" : "DISABLED (headless)") << "\n";

  // 그래프 읽기
  std::string graph_config_contents;
  absl::Status fs = mediapipe::file::GetContents(graph_path, &graph_config_contents);
  if (!fs.ok()) { std::cerr << fs.message() << "\n"; return 1; }

  mediapipe::CalculatorGraphConfig config;
  if (!google::protobuf::TextFormat::ParseFromString(graph_config_contents, &config)) {
    std::cerr << "Failed to parse graph config from pbtxt: " << graph_path << "\n";
    return 1;
  }

  CalculatorGraph graph;
  absl::Status st = graph.Initialize(config);
  if (!st.ok()) { std::cerr << st.message() << "\n"; return 1; }

  // landmarks 스트림 이름
  const std::vector<std::string> kCandidateStreams = {"multi_hand_landmarks","hand_landmarks","landmarks"};
  std::string landmarks_stream_used;

  // 최신 결과 저장
  std::mutex latest_mu;
  std::vector<NormalizedLandmarkList> latest_hands;
  bool has_latest = false;

  // 콜백 등록
  bool observe_ok = false;
  for (const auto& name : kCandidateStreams) {
    absl::Status obs = graph.ObserveOutputStream(
      name,
      [&](const Packet& p)->absl::Status {
        const auto& v = p.Get<std::vector<NormalizedLandmarkList>>();
        std::lock_guard<std::mutex> lk(latest_mu);
        latest_hands = v;
        has_latest = true;
        return absl::OkStatus();
      }
    );
    if (obs.ok()) { landmarks_stream_used = name; observe_ok = true; break; }
  }
  if (!observe_ok) {
    std::cerr << "No landmarks stream found (tried multi_hand_landmarks, hand_landmarks, landmarks)\n";
    return 1;
  }
  std::cout << "[INFO] Using landmarks stream: " << landmarks_stream_used << "\n";

  const std::string kInput = "input_video";

  // 카메라 열기
  cv::VideoCapture cap;
  int cam_w=0, cam_h=0;
  if (!OpenCameraAuto(cap, cam_w, cam_h, req_w, req_h, req_fps)) {
    std::cerr << "Cannot open camera (all backends tried)\n"; return 1;
  }

  // 그래프 시작
  st = graph.StartRun({});
  if (!st.ok()) { std::cerr << st.message() << "\n"; return 1; }

  // 창 생성 (리사이즈 가능 + 초기 크기 확대 예시)
  const std::string kWin = "Hand -> X(0~255)";
  if (gui_ok) {
    cv::namedWindow(kWin, cv::WINDOW_NORMAL);
    cv::resizeWindow(kWin, std::max(1280, cam_w), std::max(720, cam_h));
  }

  // FPS 측정기
  FpsMeter fpsm;

  // 루프
  cv::Mat frame_bgr;
  while (true) {
    if (!cap.read(frame_bgr) || frame_bgr.empty()) {
      cv::waitKey(1);
      continue;
    }
    // 캡처가 다른 크기를 주면 일관성 위해 맞춤
    if (frame_bgr.cols != cam_w || frame_bgr.rows != cam_h) {
      cv::resize(frame_bgr, frame_bgr, cv::Size(cam_w, cam_h), 0, 0, cv::INTER_AREA);
    }

    // 실시간 타임스탬프
    int64_t ts_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    // MediaPipe 제출
    auto input_frame = MatToImageFrameRGB(frame_bgr);
    Packet packet = mediapipe::Adopt(input_frame.release()).At(mediapipe::Timestamp(ts_us));
    st = graph.AddPacketToInputStream(kInput, packet);
    if (!st.ok()) { std::cerr << "[MP] AddPacket fail: " << st.message() << "\n"; break; }

    // 최신 결과 복사
    std::vector<NormalizedLandmarkList> hands_copy;
    {
      std::lock_guard<std::mutex> lk(latest_mu);
      if (has_latest) hands_copy = latest_hands;
    }

    // 손 처리 + 라벨
    int primary_x255 = -1;
    for (size_t i = 0; i < hands_copy.size(); ++i) {
      const auto& lm = hands_copy[i];
      auto palm = ComputePalmCenterPx(lm, frame_bgr.cols, frame_bgr.rows);
      if (palm.x >= 0 && palm.y >= 0) {
        int x255 = MapXTo255(palm.x, frame_bgr.cols);
        if (i == 0) primary_x255 = x255;

        // 오버레이(손바닥)
        double scale = std::max(1.0, frame_bgr.cols / 640.0);
        int radius = int(8 * scale);
        int thick  = std::max(1, int(2 * scale));
        double font = 0.7 * scale;

        cv::circle(frame_bgr, palm, radius, cv::Scalar(0,255,0), -1);
        char buf[64]; snprintf(buf, sizeof(buf), "hand%zu X255=%d", i, x255);
        cv::putText(frame_bgr, buf, palm + cv::Point2f(10,-10),
                    cv::FONT_HERSHEY_SIMPLEX, font, cv::Scalar(0,255,0), thick);
      }
    }

    // 스케일 바 + stdout 출력
    if (primary_x255 >= 0) {
      DrawScaleBar(frame_bgr, primary_x255);

      std::ostringstream os;
      os << "X255=" << primary_x255;
      if (hands_copy.size() > 1) {
        os << " others=[";
        for (size_t i=1;i<hands_copy.size();++i) {
          auto palm = ComputePalmCenterPx(hands_copy[i], frame_bgr.cols, frame_bgr.rows);
          int x255 = (palm.x>=0) ? MapXTo255(palm.x, frame_bgr.cols) : -1;
          os << (i>1?", ":"") << "h" << i << ":" << x255;
        }
        os << "]";
      }
      std::cout << os.str() << std::endl; // flush
    }

    // 프레임/캡션
    double fps_now = fpsm.update();
    DrawDisplayFrame(frame_bgr, primary_x255, fps_now);

    // 표시
    if (gui_ok) {
      cv::imshow(kWin, frame_bgr);
      int key = cv::waitKey(1);
      if (key == 27) break; // ESC
    }
  }

  graph.CloseInputStream(kInput);
  graph.WaitUntilDone();
  return 0;
}
