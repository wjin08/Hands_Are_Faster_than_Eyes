// palm_center_show.cpp
#include <opencv2/opencv.hpp>
#include <iostream>
using namespace cv;
using namespace std;

int main() {
    VideoCapture cap(0);
    if (!cap.isOpened()) { cerr << "cam open fail\n"; return -1; }

    const Scalar YCrCb_low(0, 133, 77);
    const Scalar YCrCb_high(255, 173, 127);
    Mat frame, ycrcb, mask, morph;

    while (true) {
        if (!cap.read(frame) || frame.empty()) break;

        // 피부색 영역 추출
        cvtColor(frame, ycrcb, COLOR_BGR2YCrCb);
        inRange(ycrcb, YCrCb_low, YCrCb_high, mask);

        // 모폴로지 정제
        Mat k = getStructuringElement(MORPH_ELLIPSE, Size(5,5));
        morphologyEx(mask, morph, MORPH_OPEN, k);
        morphologyEx(morph, morph, MORPH_CLOSE, k, Point(-1,-1), 2);

        // 컨투어 탐색
        vector<vector<Point>> contours;
        findContours(morph.clone(), contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
        if (!contours.empty()) {
            size_t mi = 0; double ma = 0;
            for (size_t i=0;i<contours.size();++i){
                double a = contourArea(contours[i]);
                if (a > ma) { ma = a; mi = i; }
            }

            if (ma > 1000) {
                // Distance Transform으로 손바닥 중심 추정
                Mat handMask = Mat::zeros(morph.size(), CV_8U);
                drawContours(handMask, contours, (int)mi, Scalar(255), FILLED);

                Mat dist;
                distanceTransform(handMask, dist, DIST_L2, 5);
                double maxVal; Point center;
                minMaxLoc(dist, nullptr, &maxVal, nullptr, &center);

                // 좌표 출력 및 화면 표시
                cout << "center: " << center.x << ", " << center.y << endl;
                circle(frame, center, 8, Scalar(0,255,255), FILLED);
            }
        }

        imshow("camera", frame);
        if ((waitKey(1) & 0xFF) == 'q') break;
    }
    return 0;
}
