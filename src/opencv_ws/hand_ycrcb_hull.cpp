// hand_ycrcb_hull.cpp
#include <opencv2/opencv.hpp>
#include <iostream>
#include <cmath>
using namespace cv;
using namespace std;

static inline double angleBetween(const Point& s, const Point& f, const Point& e) {
    // 각도 계산: s-f-e
    double a = norm(Mat(s - f));
    double b = norm(Mat(e - f));
    double c = norm(Mat(e - s));
    if (a <= 1e-5 || b <= 1e-5) return 180.0;
    double cosv = (a*a + b*b - c*c) / (2*a*b);
    cosv = std::max(-1.0, std::min(1.0, cosv));
    return std::acos(cosv) * 180.0 / CV_PI;
}

int main() {
    VideoCapture cap(0);
    if (!cap.isOpened()) {
        cerr << "카메라 오픈 실패\n";
        return -1;
    }

    Mat frame, ycrcb, mask, morph;
    const Scalar YCrCb_low(0, 133, 77);     // 피부색 하한
    const Scalar YCrCb_high(255, 173, 127); // 피부색 상한

    while (true) {
        if (!cap.read(frame) || frame.empty()) break;

        // 1) YCrCb 피부색 마스크
        cvtColor(frame, ycrcb, COLOR_BGR2YCrCb);
        inRange(ycrcb, YCrCb_low, YCrCb_high, mask);

        // 2) 노이즈 제거
        Mat kernel = getStructuringElement(MORPH_ELLIPSE, Size(5,5));
        morphologyEx(mask, morph, MORPH_OPEN, kernel, Point(-1,-1), 1);
        morphologyEx(morph, morph, MORPH_CLOSE, kernel, Point(-1,-1), 2);

        // 3) 컨투어 탐색
        vector<vector<Point>> contours;
        vector<Vec4i> hierarchy;
        findContours(morph, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
        if (contours.empty()) {
            imshow("mask", morph);
            imshow("result", frame);
            if ((waitKey(1) & 0xFF) == 'q') break;
            continue;
        }

        // 가장 큰 컨투어 선택
        size_t bestIdx = 0;
        double bestArea = 0.0;
        for (size_t i = 0; i < contours.size(); ++i) {
            double a = contourArea(contours[i]);
            if (a > bestArea) { bestArea = a; bestIdx = i; }
        }
        vector<Point> cnt = contours[bestIdx];
        if (cnt.size() < 5) { // 너무 작으면 스킵
            imshow("mask", morph);
            imshow("result", frame);
            if ((waitKey(1) & 0xFF) == 'q') break;
            continue;
        }

        // 4) 근사 + Convex Hull
        vector<Point> approx;
        approxPolyDP(cnt, approx, 0.01 * arcLength(cnt, true), true);

        vector<int> hullIdx;
        convexHull(approx, hullIdx, false, false); // index 용
        vector<Point> hullPts;
        convexHull(approx, hullPts, false, true);  // 좌표 용

        // 5) Convexity Defects로 손가락 후보 추출
        vector<Vec4i> defects;
        if (hullIdx.size() > 3)
            convexityDefects(approx, hullIdx, defects);

        // 그리기
        Mat out = frame.clone();
        drawContours(out, vector<vector<Point>>{approx}, -1, Scalar(0,255,0), 2);
        polylines(out, hullPts, true, Scalar(0,0,255), 2);

        // 손가락 끝점 필터링
        vector<Point> fingertips;
        for (const auto& d : defects) {
            int s = d[0], e = d[1], f = d[2];
            float depth = d[3] / 256.0f;              // 픽셀 단위 깊이
            if (s < 0 || e < 0 || f < 0) continue;
            Point ps = approx[s], pe = approx[e], pf = approx[f];

            double ang = angleBetween(ps, pf, pe);    // 오목부 각도
            if (depth > 10.0 && ang < 90.0) {        // 경험적 임계값
                // 보통 시작점/끝점이 끝점 후보
                fingertips.push_back(ps);
                fingertips.push_back(pe);
            }
        }

        // 중복 제거 및 화면 윗부분 쪽만 선택(손목 제외) 등 간단 정제
        // 중심 계산
        Moments m = moments(approx);
        Point2f center( (float)(m.m10 / max(m.m00, 1e-5)), (float)(m.m01 / max(m.m00, 1e-5)) );

        // 가까운 점 제거
        const int minDist = 20;
        vector<Point> uniqueTips;
        for (const auto& p : fingertips) {
            bool keep = true;
            for (const auto& q : uniqueTips)
                if (norm(p - q) < minDist) { keep = false; break; }
            if (keep && p.y < center.y) // 손목 아래쪽 제거
                uniqueTips.push_back(p);
        }

        // 시각화
        circle(out, center, 5, Scalar(255,0,0), FILLED);
        for (const auto& p : uniqueTips)
            circle(out, p, 8, Scalar(0,255,255), FILLED);

        putText(out, format("fingers=%zu", uniqueTips.size()),
                Point(10,30), FONT_HERSHEY_SIMPLEX, 1.0, Scalar(0,255,0), 2);

        imshow("mask", morph);
        imshow("result", out);

        int k = waitKey(1) & 0xFF;
        if (k == 'q' || k == 27) break;
    }
    return 0;
}
