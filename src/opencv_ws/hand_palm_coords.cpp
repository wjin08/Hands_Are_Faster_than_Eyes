#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "mediapipe/framework/calculator_graph.h"
#include "mediapipe/framework/formats/image_frame.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/framework/port/parse_text_proto.h"
#include "mediapipe/framework/port/opencv_highgui_inc.h"
#include "mediapipe/framework/port/opencv_imgproc_inc.h"
#include "mediapipe/framework/port/opencv_video_inc.h"
#include "mediapipe/framework/formats/landmark.pb.h"
#include "mediapipe/framework/port/commandlineflags.h"

constexpr int PALM_IDX[5] = {0, 5, 9, 13, 17}; // wrist, MCP들

absl::Status RunGraph(const std::string& graph_config_path, int cam_index) {
  // 그래프 로드
  std::ifstream ifs(graph_config_path);
  if (!ifs.is_open()) return absl::NotFoundError("graph not found");
  std::string graph_config_str((std::istreambuf_iterator<char>(ifs)),
                               std::istreambuf_iterator<char>());
  mediapipe::CalculatorGraphConfig config =
      mediapipe::ParseTextProtoOrDie<mediapipe::CalculatorGraphConfig>(graph_config_str);

  mediapipe::CalculatorGraph graph;
  MP_RETURN_IF_ERROR(graph.Initialize(config));

  // 출력 스트림 Poller
  ASSIGN_OR_RETURN(auto landmark_poller,
                   graph.AddOutputStreamPoller("multi_hand_landmarks"));
  ASSIGN_OR_RETURN(auto video_poller,
                   graph.AddOutputStreamPoller("output_video"));

  MP_RETURN_IF_ERROR(graph.StartRun({}));

  // 카메라
  cv::VideoCapture cap(cam_index);
  if (!cap.isOpened()) return absl::InternalError("camera open failed");

  cv::Mat frame, frame_rgb;
  while (true) {
    cap >> frame;
    if (frame.empty()) break;

    cv::cvtColor(frame, frame_rgb, cv::COLOR_BGR2RGB);
    // ImageFrame 래핑
    auto input_frame = absl::make_unique<mediapipe::ImageFrame>(
        mediapipe::ImageFormat::SRGB, frame_rgb.cols, frame_rgb.rows,
        mediapipe::ImageFrame::kDefaultAlignment);
    std::memcpy(input_frame->MutablePixelData(), frame_rgb.data,
                frame_rgb.total() * frame_rgb.elemSize());

    // 그래프에 전송
    MP_RETURN_IF_ERROR(graph.AddPacketToInputStream(
        "input_video",
        mediapipe::Adopt(input_frame.release())
            .At(mediapipe::Timestamp::NextAllowedInStream())));

    // 출력 수신
    mediapipe::Packet video_packet;
    if (video_poller.Next(&video_packet)) {
      auto& output_frame = video_packet.Get<mediapipe::ImageFrame>();
      cv::Mat out_mat = mediapipe::formats::MatView(&output_frame);
      cv::Mat out_bgr;
      cv::cvtColor(out_mat, out_bgr, cv::COLOR_RGB2BGR);

      // 랜드마크 수신 및 손바닥 중심 계산
      mediapipe::Packet lm_packet;
      if (landmark_poller.QueueSize() > 0 && landmark_poller.Next(&lm_packet)) {
        const auto& lists =
            lm_packet.Get<std::vector<mediapipe::NormalizedLandmarkList>>();
        int h = out_bgr.rows, w = out_bgr.cols;

        for (size_t i = 0; i < lists.size(); ++i) {
          const auto& lms = lists[i];
          if (lms.landmark_size() < 21) continue;

          float sx = 0.f, sy = 0.f;
          for (int k = 0; k < 5; ++k) {
            const auto& lm = lms.landmark(PALM_IDX[k]);
            sx += lm.x() * w;
            sy += lm.y() * h;
          }
          int cx = static_cast<int>(sx / 5.f);
          int cy = static_cast<int>(sy / 5.f);

          cv::circle(out_bgr, {cx, cy}, 6, {0, 255, 0}, -1);
          cv::putText(out_bgr, "Palm(" + std::to_string(cx) + "," + std::to_string(cy) + ")",
                      {10, 30 + static_cast<int>(i)*25}, cv::FONT_HERSHEY_SIMPLEX, 0.7,
                      {0, 255, 0}, 2);
        }
      }

      cv::imshow("Palm coords (pixels, origin=top-left)", out_bgr);
    }

    if (cv::waitKey(1) == 27) break; // ESC
  }

  MP_RETURN_IF_ERROR(graph.CloseInputStream("input_video"));
  return graph.WaitUntilDone();
}

DEFINE_string(graph, "hand_palm_tracking.pbtxt", "CalculatorGraph config path");
DEFINE_int32(cam, 0, "camera index");

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  auto status = RunGraph(FLAGS_graph, FLAGS_cam);
  if (!status.ok()) {
    std::cerr << status.message() << std::endl;
    return 1;
  }
  return 0;
}
