/* 서울기술 교육센터 IoT */
/* author : KSH */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>

#include <opencv2/opencv.hpp>
#include <opencv2/core/cuda.hpp>      // ← 추가
#include <iostream>

#define BUF_SIZE  1025
#define NAME_SIZE 20
#define ARR_CNT 5

using namespace cv;
using namespace std;

void* send_msg(void* arg);
void* recv_msg(void* arg);
void* opencv_coordinate(void* arg);
void error_handling(const char* msg);

char name[NAME_SIZE]="[Default]";
char msg[BUF_SIZE];

int main(int argc, char *argv[])
{
	int sock;
	struct sockaddr_in serv_addr;
	pthread_t snd_thread, rcv_thread, opencv_thread;
	void * thread_return;

	if(argc != 4) {
		printf("Usage : %s <IP> <port> <name>\n",argv[0]);
		exit(1);
	}

	sprintf(name, "%s",argv[3]);

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if(sock == -1)
		error_handling("socket() error");

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(atoi(argv[2]));

	if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
		error_handling("connect() error");

	sprintf(msg,"[%s:PASSWD]",name);
	(void)write(sock, msg, strlen(msg));
	pthread_create(&rcv_thread, NULL, recv_msg, (void *)&sock);
	pthread_create(&snd_thread, NULL, send_msg, (void *)&sock);
    pthread_create(&opencv_thread, NULL, opencv_coordinate, (void *)&sock);

	pthread_join(snd_thread, &thread_return);
    pthread_join(opencv_thread, &thread_return); 
	//	pthread_join(rcv_thread, &thread_return);

	close(sock);
	return 0;
}

void * send_msg(void * arg)
{
	int *sock = (int *)arg;
	int str_len;
	int ret;
	fd_set initset, newset;
	struct timeval tv;
	char name_msg[NAME_SIZE + BUF_SIZE+2];

	FD_ZERO(&initset);
	FD_SET(STDIN_FILENO, &initset);

	fputs("Input a message! [ID]msg (Default ID:ALLMSG)\n",stdout);
	while(1) {
		memset(msg,0,sizeof(msg));
		name_msg[0] = '\0';
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		newset = initset;
		ret = select(STDIN_FILENO + 1, &newset, NULL, NULL, &tv);
		if(FD_ISSET(STDIN_FILENO, &newset))
		{
			fgets(msg, BUF_SIZE, stdin);
			if(!strncmp(msg,"quit\n",5)) {
				*sock = -1;
				return NULL;
			}
			else if(msg[0] != '[')
			{
				strcat(name_msg,"[ALLMSG]");
				strcat(name_msg,msg);
			}
			else
				strcpy(name_msg,msg);
			if(write(*sock, name_msg, strlen(name_msg))<=0)
			{
				*sock = -1;
				return NULL;
			}
		}
		if(ret == 0)
		{
			if(*sock == -1)
				return NULL;
		}
	}
}

void * recv_msg(void * arg)
{
	int * sock = (int *)arg;
	int i;
	char *pToken;
	char *pArray[ARR_CNT]={0};

	char name_msg[NAME_SIZE + BUF_SIZE +1];
	int str_len;
	while(1) {
		memset(name_msg,0x0,sizeof(name_msg));
		str_len = read(*sock, name_msg, NAME_SIZE + BUF_SIZE );
		if(str_len <= 0)
		{
			*sock = -1;
			return NULL;
		}
		name_msg[str_len] = 0;
		fputs(name_msg, stdout);

		/*   	pToken = strtok(name_msg,"[:]");
			i = 0;
			while(pToken != NULL)
			{
			pArray[i] =  pToken;
			if(i++ >= ARR_CNT)
			break;
			pToken = strtok(NULL,"[:]");
			}

		//		printf("id:%s, msg:%s,%s,%s,%s\n",pArray[0],pArray[1],pArray[2],pArray[3],pArray[4]);
		printf("id:%s, msg:%s\n",pArray[0],pArray[1]);
		*/
	}
}
void* opencv_coordinate(void* arg)
{
	int* sock = (int*) arg;
    cout << "Have CUDA : " << cv::cuda::getCudaEnabledDeviceCount() << endl;
    if (cv::cuda::getCudaEnabledDeviceCount() > 0) {
        cv::cuda::printShortCudaDeviceInfo(cv::cuda::getDevice());
    }
    VideoCapture cap(0);
    if (!cap.isOpened()) { cerr << "cam open fail\n"; return nullptr; }

    const Scalar YCrCb_low(0,133,77), YCrCb_high(255,173,127);
    Mat frame,ycrcb,mask,morph;

    while (true) {
        if (!cap.read(frame) || frame.empty()) break;

        // GPU 전처리 사용하려면 cudaimgproc 헤더/링크도 필요
        // (지금은 CPU 경로 그대로 유지)
        cvtColor(frame, ycrcb, COLOR_BGR2YCrCb);
        inRange(ycrcb, YCrCb_low, YCrCb_high, mask);
        Mat k = getStructuringElement(MORPH_ELLIPSE, Size(5,5));
        morphologyEx(mask, morph, MORPH_OPEN, k);
        morphologyEx(morph, morph, MORPH_CLOSE, k, Point(-1,-1), 2);

        // 중심 계산(이전 코드와 동일)
        vector<vector<Point>> contours;
        findContours(morph.clone(), contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
        if (!contours.empty()) {
            size_t mi=0; double ma=0;
            for (size_t i=0;i<contours.size();++i){ double a=contourArea(contours[i]); if(a>ma){ma=a; mi=i;} }
            if (ma>1000) {
                Mat hand = Mat::zeros(morph.size(), CV_8U);
                drawContours(hand, contours, (int)mi, Scalar(255), FILLED);
                Mat dist; distanceTransform(hand, dist, DIST_L2, 5);
                double maxVal; Point center;
                minMaxLoc(dist, nullptr, &maxVal, nullptr, &center);
                circle(frame, center, 8, Scalar(0,255,255), FILLED);
                cout << "center: " << center.x << ", " << center.y << endl;

                char buf[32];
                int n = snprintf(buf, sizeof(buf), "%d\n", center.x); // 개행은 라인 구분용
                if (n > 0) {
                    if (write(*sock, buf, (size_t)n) <= 0) {  // 에러 시 종료
                        *sock = -1;
                        break;
						return nullptr;
                    }
                }
            }
        }
        imshow("camera", frame);
        if ((waitKey(1) & 0xFF) == 'q') break;
        if (*sock == -1) break;
    }
    return nullptr;
}
void error_handling(const char * msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}