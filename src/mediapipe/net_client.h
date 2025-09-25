#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <iomanip>
#include <sstream>

class NetClient {
public:
  NetClient(std::string server_ip, int server_port,
            std::string my_id, std::string my_pw,
            std::string to_id = "KSH_QT");
  ~NetClient();

  void start();
  void stop();

  // 숫자 전송 요청(내부 큐에 적재; 실제 송신은 전송 스레드가 수행)
  void send_value(int value); // 0~255

private:
  // queue
  void push_(std::string s);
  bool pop_wait_(std::string& out);

  // thread routine
  void sender_thread_();
  int  connect_and_auth_();

private:
  // config
  const std::string ip_;
  const int         port_;
  const std::string my_id_;
  const std::string my_pw_;
  const std::string to_id_;

  // state
  std::atomic<bool> running_{false};
  std::thread thr_;

  // queue
  std::mutex mu_;
  std::condition_variable cv_;
  std::deque<std::string> q_;
};
