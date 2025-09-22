// fingertips.cpp
#include <opencv2/opencv.hpp>
#include <iostream>
using namespace cv;
using namespace std;

static inline double angleBetween(const Point& s, const Point& f, const Point& e) {
    double a = norm(s - f), b = norm(e - f), c = norm(e - s);
    if (a < 1e-5 || b < 1e-5) return 180.0;
    double cosv = (a*a + b*b - c*c) / (2*a*b);
    cosv = max(-1.0, min(1.0, cosv));
    return acos(cosv) * 180.0 / CV_PI;
}

int main() {
    VideoCapture cap(0);
    if (!cap.isOpened()) { cerr << "cam open fail\n"; return -1; }

    const Scalar YCrCb_low(0, 133, 77), YCrCb_high(255, 173, 127);
    Mat frame, ycrcb, mask, morph;

    while (true) {
        if (!cap.read(frame) || frame.empty()) break;

        // 피부 마스크
        cvtColor(frame, ycrcb, COLOR_BGR2YCrCb);
        inRange(ycrcb, YCrCb_low, YCrCb_high, mask);
        Mat k = getStructuringElement(MORPH_ELLIPSE, Size(5,5));
        morphologyEx(mask, morph, MORPH_OPEN, k);
        morphologyEx(morph, morph, MORPH_CLOSE, k, Point(-1,-1), 2);

        // 가장 큰 컨투어
        vector<vector<Point>> contours;
        findContours(morph.clone(), contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
        Mat out = frame.clone();
        if (!contours.empty()) {
            size_t mi=0; double ma=0;
            for (size_t i=0;i<contours.size();++i){ double a=contourArea(contours[i]); if(a>ma){ma=a; mi=i;} }
            if (ma > 1000 && contours[mi].size() >= 5) {
                vector<Point> cnt = contours[mi], approx;
                approxPolyDP(cnt, approx, 0.01 * arcLength(cnt, true), true);

                // 중심(손목 제거용 기준)
                Moments mm = moments(approx);
                Point2f c( (float)(mm.m10/max(mm.m00,1e-5)), (float)(mm.m01/max(mm.m00,1e-5)) );
                circle(out, c, 4, Scalar(255,0,0), FILLED);

                // Hull + Defects
                vector<int> hullIdx;
                convexHull(approx, hullIdx, false, false);
                vector<Vec4i> defects;
                if (hullIdx.size() > 3) convexityDefects(approx, hullIdx, defects);

                drawContours(out, vector<vector<Point>>{approx}, -1, Scalar(0,255,0), 2);

                // 결함 기반 끝점 후보 수집
                vector<Point> tips;
                for (const auto& d : defects) {
                    int s=d[0], e=d[1], f=d[2];
                    float depth = d[3] / 256.0f;
                    Point ps = approx[s], pe = approx[e], pf = approx[f];
                    double ang = angleBetween(ps, pf, pe);
                    if (depth > 10.0f && ang < 90.0) { // 튜닝 파라미터
                        tips.push_back(ps);
                        tips.push_back(pe);
                    }
                }

                // 중복 제거 + 손목 필터링
                vector<Point> uniq;
                const int minDist = 20;
                for (const auto& p : tips) {
                    if (p.y >= c.y) continue; // 손목/하단 제거
                    bool keep = true;
                    for (const auto& q : uniq) if (norm(p - q) < minDist) { keep = false; break; }
                    if (keep) uniq.push_back(p);
                }

                // 출력 및 표시
                cout << "tips:";
                for (const auto& p : uniq) {
                    cout << " " << p.x << " " << p.y;
                    circle(out, p, 7, Scalar(0,255,255), FILLED);
                }
                cout << "\n";
            }
        }

        imshow("fingertips", out);
        int kkey = waitKey(1) & 0xFF;
        if (kkey == 'q' || kkey == 27) break;
    }
    return 0;
}
