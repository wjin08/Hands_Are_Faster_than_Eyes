#include "hand_tracker.h"

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <mutex>
#include <atomic>
#include <cmath>
#include <sstream>
#include <thread>

// OpenCV
#include <opencv2/opencv.hpp>

// MediaPipe
#include "mediapipe/framework/calculator_graph.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/framework/port/file_helpers.h"

#include <google/protobuf/text_format.h>

using ::mediapipe::CalculatorGraph;
using ::mediapipe::ImageFrame;
using ::mediapipe::Packet;
using ::mediapipe::NormalizedLandmarkList;

namespace {

// util
std::unique_ptr<ImageFrame> MatToImageFrameRGB(const cv::Mat& bgr) {
  auto frame = std::make_unique<ImageFrame>(
    mediapipe::ImageFormat::SRGB, bgr.cols, bgr.rows,
    ImageFrame::kDefaultAlignmentBoundary);
  cv::Mat dst(frame->Height(), frame->Width(), CV_8UC3, frame->MutablePixelData());
  cv::cvtColor(bgr, dst, cv::COLOR_BGR2RGB);
  return frame;
}

cv::Point2f ComputePalmCenterPx(const NormalizedLandmarkList& lm, int w, int h) {
  static const int idxs[] = {0, 1, 5, 9, 13, 17};
  float sx = 0.f, sy = 0.f; int cnt = 0;
  for (int i : idxs) if (i < lm.landmark_size()) {
    const auto& p = lm.landmark(i);
    sx += p.x() * w; sy += p.y() * h; ++cnt;
  }
  if (cnt == 0) return {-1.f, -1.f};
  return {sx / cnt, sy / cnt};
}

int MapXTo255(float x_px, int width) {
  if (width <= 1) return 0;
  float v = (x_px / (float)(width - 1)) * 255.0f;
  v = std::max(0.0f, std::min(255.0f, v));
  return (int)std::lround(v);
}

void DrawScaleBar(cv::Mat& img, int value255) {
  const int margin = 10;
  const int bar_h = std::max(24, img.rows / 20);
  int y0 = img.rows - bar_h - margin;
  cv::rectangle(img, {margin, y0}, {img.cols - margin, y0 + bar_h}, cv::Scalar(30,30,30), cv::FILLED);
  const int ticks[] = {0,64,128,192,255};
  for (int t : ticks) {
    int x = margin + (img.cols - 2*margin) * t / 255;
    cv::line(img, {x, y0}, {x, y0 + bar_h}, cv::Scalar(80,80,80), 1);
    char buf[8]; snprintf(buf, sizeof(buf), "%d", t);
    cv::putText(img, buf, {x+2, y0 - 4}, cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(200,200,200), 1);
  }
  int xv = margin + (img.cols - 2*margin) * value255 / 255;
  cv::line(img, {xv, y0 - 6}, {xv, y0 + bar_h + 6}, cv::Scalar(0,255,255), 2);
  char label[32]; snprintf(label, sizeof(label), "X255:%d", value255);
  cv::putText(img, label, {margin, y0 - 10}, cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0,255,255), 2);
}

void DrawFpsOverlay(cv::Mat& img, double fps) {
  if (img.empty()) return;
  char text[64]; std::snprintf(text, sizeof(text), "FPS: %.1f", fps);
  int baseline = 0; double fontScale = 0.7; int thickness = 2;
  cv::Size ts = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, fontScale, thickness, &baseline);
  int pad = 8, box_w = ts.width + pad * 2, box_h = ts.height + pad * 2, x = 10, y = 10;
  box_w = std::min(box_w, img.cols - x); box_h = std::min(box_h, img.rows - y);
  if (box_w <= 0 || box_h <= 0) return;
  cv::Rect box(x, y, box_w, box_h);
  cv::Mat roi = img(box), overlay; roi.copyTo(overlay);
  cv::rectangle(overlay, cv::Rect(0,0,box_w,box_h), cv::Scalar(0,0,0), cv::FILLED);
  cv::addWeighted(overlay, 0.35, roi, 0.65, 0.0, roi);
  cv::putText(img, text, {x + pad, y + box_h - pad - baseline},
              cv::FONT_HERSHEY_SIMPLEX, fontScale, cv::Scalar(255,255,255), thickness);
}

struct CamTry { int index; int backend; int fourcc; const char* name; };

bool TryOpen(cv::VideoCapture& cap, const CamTry& t, int w, int h, int fps) {
  std::cerr << "[CAM] try backend=" << t.name << " idx=" << t.index << "\n";
  bool ok = (t.backend == cv::CAP_ANY) ? cap.open(t.index) : cap.open(t.index, t.backend);
  if (!ok) { std::cerr << "[CAM] open fail\n"; return false; }
  if (w>0) cap.set(cv::CAP_PROP_FRAME_WIDTH,  w);
  if (h>0) cap.set(cv::CAP_PROP_FRAME_HEIGHT, h);
  if (fps>0) cap.set(cv::CAP_PROP_FPS, fps);
  cap.set(cv::CAP_PROP_BUFFERSIZE, 1);
  if (t.fourcc) cap.set(cv::CAP_PROP_FOURCC, t.fourcc);
  cv::Mat test;
  if (!cap.read(test) || test.empty()) {
    std::cerr << "[CAM] read test frame FAIL\n";
    cap.release(); return false;
  }
  std::cerr << "[CAM] OK: " << test.cols << "x" << test.rows << "\n";
  return true;
}

bool OpenCameraAuto(cv::VideoCapture& cap, int& out_w, int& out_h,
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

static std::atomic<long long> g_last_obs_us{0};

} // namespace

// ===== HandTracker =====
HandTracker::HandTracker(int req_w, int req_h, int req_fps, bool gui,
                         std::function<void(int)> on_value)
  : req_w_(req_w), req_h_(req_h), req_fps_(req_fps), gui_(gui),
    on_value_(std::move(on_value)) {}

bool HandTracker::init(const std::string& graph_path) {
  (void)graph_path; // 실제 초기화는 run()에서 수행
  return true;
}

int HandTracker::run() {
  // MediaPipe 그래프 로드
  std::string graph_path = "mediapipe/graphs/hand_tracking/hand_tracking_desktop_live.pbtxt";
  std::string graph_config_contents;
  {
    absl::Status fs = mediapipe::file::GetContents(graph_path, &graph_config_contents);
    if (!fs.ok()) { std::cerr << fs.message() << "\n"; return 1; }
  }

  mediapipe::CalculatorGraphConfig config;
  if (!google::protobuf::TextFormat::ParseFromString(graph_config_contents, &config)) {
    std::cerr << "Failed to parse graph config from pbtxt: " << graph_path << "\n";
    return 1;
  }

  CalculatorGraph graph;
  {
    absl::Status st = graph.Initialize(config);
    if (!st.ok()) { std::cerr << st.message() << "\n"; return 1; }
  }

  const std::vector<std::string> kCandidateStreams = {"multi_hand_landmarks","hand_landmarks","landmarks"};
  std::string landmarks_stream_used;
  std::mutex latest_mu;
  std::vector<NormalizedLandmarkList> latest_hands;
  bool has_latest = false;

  bool observe_ok = false;
  for (const auto& name : kCandidateStreams) {
    absl::Status obs = graph.ObserveOutputStream(
      name,
      [&](const Packet& p)->absl::Status {
        const auto& v = p.Get<std::vector<NormalizedLandmarkList>>();
        {
          std::lock_guard<std::mutex> lk(latest_mu);
          latest_hands = v; has_latest = true;
        }
        auto tp = std::chrono::steady_clock::now();
        long long us = std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count();
        g_last_obs_us.store(us, std::memory_order_relaxed);
        return absl::OkStatus();
      }
    );
    if (obs.ok()) { landmarks_stream_used = name; observe_ok = true; break; }
  }
  if (!observe_ok) { std::cerr << "No landmarks stream found\n"; return 1; }
  std::cout << "[INFO] Using landmarks stream: " << landmarks_stream_used << "\n";

  const std::string kInput = "input_video";

  cv::VideoCapture cap;
  int cam_w=0, cam_h=0;
  if (!OpenCameraAuto(cap, cam_w, cam_h, req_w_, req_h_, req_fps_)) {
    std::cerr << "Cannot open camera (all backends tried)\n"; return 1;
  }

  {
    absl::Status st = graph.StartRun({});
    if (!st.ok()) { std::cerr << st.message() << "\n"; return 1; }
  }

  const char* kWinName = "Hand -> X(0~255)";
  if (gui_) { cv::namedWindow(kWinName, cv::WINDOW_NORMAL); cv::resizeWindow(kWinName, 1280, 1000); }

  int frames = 0;
  auto t0 = std::chrono::steady_clock::now();
  double fps_display = 0.0;
  auto last_frame_tp = std::chrono::steady_clock::now();

  cv::Mat frame_bgr;
  auto last_send_tp = std::chrono::steady_clock::now();

  while (true) {
    if (!cap.read(frame_bgr) || frame_bgr.empty()) { cv::waitKey(1); continue; }
    if (frame_bgr.cols != cam_w || frame_bgr.rows != cam_h) {
      cv::resize(frame_bgr, frame_bgr, cv::Size(cam_w, cam_h), 0, 0, cv::INTER_AREA);
    }

    auto nowtp = std::chrono::steady_clock::now();
    int64_t now_us = std::chrono::duration_cast<std::chrono::microseconds>(nowtp.time_since_epoch()).count();

    // MediaPipe 제출
    auto input_frame = MatToImageFrameRGB(frame_bgr);
    Packet packet = mediapipe::Adopt(input_frame.release()).At(mediapipe::Timestamp(now_us));
    {
      absl::Status st = graph.AddPacketToInputStream(kInput, packet);
      if (!st.ok()) { std::cerr << "[MP] AddPacket fail: " << st.message() << "\n"; break; }
    }

    // 최신 결과 복사
    std::vector<NormalizedLandmarkList> hands_copy;
    {
      std::lock_guard<std::mutex> lk(latest_mu);
      if (has_latest) hands_copy = latest_hands;
    }

    // 신선도 체크
    constexpr long long kFreshThreshUs = 300000; // 300ms
    bool hands_fresh = (now_us - g_last_obs_us.load(std::memory_order_relaxed)) <= kFreshThreshUs;
    bool hands_visible = hands_fresh && !hands_copy.empty();

    int primary_x255 = -1;

    if (hands_visible) {
      for (size_t i = 0; i < hands_copy.size(); ++i) {
        const auto& lm = hands_copy[i];
        auto palm = ComputePalmCenterPx(lm, frame_bgr.cols, frame_bgr.rows);
        if (palm.x >= 0 && palm.y >= 0) {
          int x255 = MapXTo255(palm.x, frame_bgr.cols);
          if (i == 0) primary_x255 = x255;

          if (gui_) {
            cv::circle(frame_bgr, palm, 8, cv::Scalar(0,255,0), -1);
            char buf[64]; snprintf(buf, sizeof(buf), "hand%zu X255=%d", i, x255);
            cv::putText(frame_bgr, buf, palm + cv::Point2f(10,-10),
                        cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0,255,0), 2);
          }
        }
      }
      if (gui_ && primary_x255 >= 0) {
        DrawScaleBar(frame_bgr, primary_x255);
      }
    }

    // 15Hz 레이트로 콜백 호출(손 없으면 0)
    if (std::chrono::duration<double>(nowtp - last_send_tp).count() >= (1.0/15.0)) {
      last_send_tp = nowtp;
      int value_to_send = (hands_visible && primary_x255 >= 0) ? primary_x255 : 0;
      if (on_value_) on_value_(value_to_send);
    }

    // FPS overlay
    double inst_dt = std::chrono::duration<double>(nowtp - last_frame_tp).count();
    last_frame_tp = nowtp;
    double inst_fps = (inst_dt > 0.0) ? (1.0 / inst_dt) : 0.0;
    if (fps_display <= 0.0) fps_display = inst_fps;
    else fps_display = 0.2 * inst_fps + 0.8 * fps_display;

    if (std::chrono::duration_cast<std::chrono::seconds>(nowtp - t0).count() >= 1) {
      std::cerr << "[FPS] ~" << frames << " fps\n";
      frames = 0; t0 = nowtp;
    } else {
      frames++;
    }

    if (gui_) {
      DrawFpsOverlay(frame_bgr, fps_display);
      cv::setWindowTitle(kWinName, (std::ostringstream() << kWinName
                        << "  |  FPS: " << std::fixed << std::setprecision(1) << fps_display).str());
      cv::imshow(kWinName, frame_bgr);
      int key = cv::waitKey(1);
      if (key == 27) break; // ESC
    }
  }

  graph.CloseInputStream("input_video");
  graph.WaitUntilDone();
  return 0;
}
