#include <opencv2/opencv.hpp>
#include <iostream>   // cout 사용시 필요
using namespace cv;
using namespace std;

int main()
{
    VideoCapture cap(0);
    if(!cap.isOpened())
    {
        cout << "카메라를 열 수 없습니다." << endl;
        return -1;
    }

    Mat frame;
    while(true)
    {
        cap >> frame;
        if(frame.empty()) break;

        imshow("camera", frame);

        if(waitKey(10) == 'q') break;
    }
    return 0;
}
