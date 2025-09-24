#include "mainwidget.h"
#include <QVBoxLayout>

MainWidget::MainWidget(QWidget *parent) 
    : QWidget(parent)
{
    // 레이아웃 설정
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // 탭 위젯 생성
    pTabWidget = new QTabWidget(this);
    
    // 탭 생성
    pTab1 = new Tab1DevControl(this);
    pTab2 = new Tab2SocketClient(this);
    
    // 탭 추가
    pTabWidget->addTab(pTab1, "Device Control");
    pTabWidget->addTab(pTab2, "Socket Client");
    
    mainLayout->addWidget(pTabWidget);
    
    // === 시그널-슬롯 연결 ===
    
    // LED 제어: Tab1 다이얼 변경 → Tab2로 전송 → 서버
    connect(pTab1, SIGNAL(ledValueChangedSig(int)), 
            pTab2, SLOT(socketSendLedData(int)));
    
    // LED 수신: 서버(다른 클라이언트 포함) → Tab2 → Tab1 다이얼 업데이트
    connect(pTab2, SIGNAL(ledWriteSig(int)), 
            pTab1, SLOT(updateLedFromServer(int)));
    
    setWindowTitle("LED Remote Control - Real-time Sync");
    resize(400, 400);
}

MainWidget::~MainWidget()
{
}
