#include "hand_tracker.h"
#include "net_client.h"
#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
  // 서버 설정 (기존 상수와 동일)
  const char* kSrvIp = "10.10.16.243";
  const int   kSrvPort = 5000;
  const char* kMyId  = "3";
  const char* kMyPw  = "PASSWD";

  bool gui = true;
  for (int i=1;i<argc;i++) {
    std::string a = argv[i];
    if (a == "--no-gui") gui = false;
    if (a == "--gui")    gui = true;
  }

  // 네트워킹 시작
  NetClient net(kSrvIp, kSrvPort, kMyId, kMyPw, "2");
  net.start();

  // HandTracker: 640x480@30, 15Hz로 net.send_value 호출 (손 없으면 0)
  HandTracker tracker(640, 480, 30, gui, [&](int v){
    net.send_value(v);
  });

  if (!tracker.init("mediapipe/graphs/hand_tracking/hand_tracking_desktop_live.pbtxt")) {
    std::cerr << "tracker init failed\n";
    return 1;
  }

  int ret = tracker.run();

  net.stop();
  return ret;
}
