/* 서울기술 교육센터 IoT */
/* author : KSH */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <opencv2/opencv.hpp>
#include <opencv2/core/cuda.hpp>
#include <iostream>
#include <algorithm> // std::clamp

#define BUF_SIZE  1025
#define NAME_SIZE 20

using namespace cv;
using namespace std;

static void error_handling(const char* msg) {
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(1);
}

char namebuf[NAME_SIZE] = "[Default]";
char msgbuf[BUF_SIZE];

int main(int argc, char *argv[])
{
    if (argc != 4) {
        printf("Usage : %s <IP> <port> <name>\n", argv[0]);
        return 1;
    }

    snprintf(namebuf, sizeof(namebuf), "%s", argv[3]);

    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) error_handling("socket() error");

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");

    // 로그인/패스워드 패킷 전송
    int passlen = snprintf(msgbuf, sizeof(msgbuf), "[%s:PASSWD]", namebuf);
    if (passlen > 0) (void)write(sock, msgbuf, (size_t)passlen);

    // CUDA 정보 출력(옵션)
    cout << "Have CUDA : " << cv::cuda::getCudaEnabledDeviceCount() << endl;
    if (cv::cuda::getCudaEnabledDeviceCount() > 0) {
        cv::cuda::printShortCudaDeviceInfo(cv::cuda::getDevice());
    }

    VideoCapture cap(0);
    if (!cap.isOpened()) { cerr << "cam open fail\n"; close(sock); return 1; }

    const Scalar YCrCb_low(0,133,77), YCrCb_high(255,173,127);
    Mat frame, ycrcb, mask, morph;

    while (true) {
        if (!cap.read(frame) || frame.empty()) break;

        cvtColor(frame, ycrcb, COLOR_BGR2YCrCb);
        inRange(ycrcb, YCrCb_low, YCrCb_high, mask);
        Mat k = getStructuringElement(MORPH_ELLIPSE, Size(5,5));
        morphologyEx(mask, morph, MORPH_OPEN, k);
        morphologyEx(morph, morph, MORPH_CLOSE, k, Point(-1,-1), 2);

        vector<vector<Point>> contours;
        findContours(morph.clone(), contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
        if (!contours.empty()) {
            size_t mi = 0; double ma = 0;
            for (size_t i=0;i<contours.size();++i){
                double a = contourArea(contours[i]);
                if (a > ma) { ma = a; mi = i; }
            }
            if (ma > 1000) {
                Mat hand = Mat::zeros(morph.size(), CV_8U);
                drawContours(hand, contours, (int)mi, Scalar(255), FILLED);
                Mat dist; distanceTransform(hand, dist, DIST_L2, 5);
                double maxVal; Point center;
                minMaxLoc(dist, nullptr, &maxVal, nullptr, &center);
                circle(frame, center, 8, Scalar(0,255,255), FILLED);

                // 0~640 → 0~255 스케일링
                int scaled = (center.x * 255) / 640;
                int val = std::clamp(scaled, 0, 255);

                char txbuf[64];
                int n = snprintf(txbuf, sizeof(txbuf), "[KSH_QT]LED@0x%02x\n", val);

                if (n > 0) {
                    std::cout << "TX: " << txbuf; // 로그
                    if (write(sock, txbuf, (size_t)n) <= 0) {
                        cerr << "write() error\n";
                        break;
                    }
                }
            }
        }

        imshow("camera", frame);
        if ((waitKey(1) & 0xFF) == 'q') break;
    }

    close(sock);
    return 0;
}
