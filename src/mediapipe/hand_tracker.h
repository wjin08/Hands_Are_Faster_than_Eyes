#pragma once
#include <functional>
#include <string>

class HandTracker {
public:
  // on_value: 15Hz로 호출, 손 없으면 0, 있으면 0~255
  HandTracker(int req_w, int req_h, int req_fps, bool gui,
              std::function<void(int)> on_value);

  // graph 설정 파일 경로
  bool init(const std::string& graph_path);
  // 루프 실행(블로킹). ESC로 종료
  int  run();

private:
  int req_w_, req_h_, req_fps_;
  bool gui_;
  std::function<void(int)> on_value_;
};
