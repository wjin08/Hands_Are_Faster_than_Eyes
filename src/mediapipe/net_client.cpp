#include "net_client.h"
#include <iostream>
#include <chrono>
#include <algorithm>

// POSIX
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

NetClient::NetClient(std::string server_ip, int server_port,
                     std::string my_id, std::string my_pw,
                     std::string to_id)
  : ip_(std::move(server_ip)), port_(server_port),
    my_id_(std::move(my_id)), my_pw_(std::move(my_pw)),
    to_id_(std::move(to_id)) {}

NetClient::~NetClient() { stop(); }

void NetClient::start() {
  if (running_.exchange(true)) return;
  thr_ = std::thread(&NetClient::sender_thread_, this);
}

void NetClient::stop() {
  if (!running_.exchange(false)) return;
  cv_.notify_all();
  if (thr_.joinable()) thr_.join();
}

void NetClient::send_value(int value) {
  if (value < 0) value = 0;
  if (value > 255) value = 255;
  push_(std::to_string(value));
}

void NetClient::push_(std::string s) {
  std::lock_guard<std::mutex> lk(mu_);
  if (q_.size() > 200) q_.pop_front();
  q_.push_back(std::move(s));
  cv_.notify_one();
}

bool NetClient::pop_wait_(std::string& out) {
  std::unique_lock<std::mutex> lk(mu_);
  cv_.wait(lk, [&]{ return !q_.empty() || !running_.load(); });
  if (!running_.load() && q_.empty()) return false;
  out = std::move(q_.front());
  q_.pop_front();
  return true;
}

int NetClient::connect_and_auth_() {
  int sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) return -1;
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port   = htons(port_);
  if (::inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr) != 1) {
    ::close(sock); return -1;
  }
  if (::connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
    ::close(sock); return -1;
  }
  // 서버는 "id:pw" (개행 없이) 기대
  std::string auth = my_id_ + ":" + my_pw_;
  ssize_t n = ::send(sock, auth.c_str(), (int)auth.size(), 0);
  if (n != (ssize_t)auth.size()) { ::close(sock); return -1; }
  return sock;
}

void NetClient::sender_thread_() {
  int sock = -1;
  int backoff_ms = 200;

  while (running_.load()) {
    if (sock < 0) {
      sock = connect_and_auth_();
      if (sock >= 0) {
        std::cerr << "[NET] connected & authed\n";
        backoff_ms = 200;
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(backoff_ms));
        backoff_ms = std::min(backoff_ms * 2, 3000);
        continue;
      }
    }

    std::string payload;
    if (!pop_wait_(payload)) break;

    // "2:<숫자>\n" 로 전송
    std::string wire = to_id_ + ":" + payload;
    if (wire.empty() || wire.back() != '\n') wire.push_back('\n');

    ssize_t n = ::send(sock, wire.c_str(), (int)wire.size(), 0);
    if (n != (ssize_t)wire.size()) {
      std::cerr << "[NET] send fail, reconnecting...\n";
      ::close(sock);
      sock = -1;
    }
  }

  if (sock >= 0) ::close(sock);
  std::cerr << "[NET] sender thread exit\n";
}
