#include "tab1devcontrol.h"
#include "ui_tab1devcontrol.h"
#include <QApplication>

Tab1DevControl::Tab1DevControl(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Tab1DevControl)
    , lcdData(0)
    , isUpdatingFromServer(false)
{
    ui->setupUi(this);
    int keyCount = ui->gridLayout->rowCount() * ui->gridLayout->columnCount();

    pQTimer = new QTimer(this);
    pQButtonGroup = new QButtonGroup(this);
    
    for(int i=0;i<ui->gridLayout->rowCount();i++)
    {
        for(int j=0;j<ui->gridLayout->columnCount();j++)
        {
            pQCheckBox[--keyCount] = dynamic_cast<QCheckBox*>(ui->gridLayout->itemAtPosition(i,j)->widget());
        }
    }
    
    pQButtonGroup->setExclusive(false);
    keyCount = ui->gridLayout->rowCount() * ui->gridLayout->columnCount();
    for(int i=0;i<keyCount;i++)
        pQButtonGroup->addButton(pQCheckBox[i],i+1);

    connect(pQTimer, SIGNAL(timeout()), this, SLOT(updateDialValueSlot()));
    
    // 다이얼 값 변경 시그널 연결
    connect(ui->pDialLed, SIGNAL(valueChanged(int)), this, SLOT(dialValueChangedSlot(int)));
    connect(ui->pDialLed, SIGNAL(valueChanged(int)), this, SLOT(updateProgressBarLedSlot(int)));
    
    connect(ui->pPBquit, SIGNAL(clicked()), qApp, SLOT(quit()));
}

Tab1DevControl::~Tab1DevControl()
{
    delete ui;
}

void Tab1DevControl::dialValueChangedSlot(int value)
{
    // 서버에서 온 업데이트가 아닌 경우에만 서버로 전송
    if (!isUpdatingFromServer)
    {
        emit ledValueChangedSig(value);
        qDebug() << "Dial changed by user:" << value;
    }
}

void Tab1DevControl::updateProgressBarLedSlot(int value)
{
    ui->pProgressBarLed->setValue(value);
    ui->pLcdNumberLed->display(value);
    
    // LED 패턴 표시를 위한 체크박스 업데이트
    int led_count;
    if (value == 0) {
        led_count = 0;
    } else if (value <= 31) {
        led_count = 1;
    } else if (value <= 63) {
        led_count = 2;
    } else if (value <= 95) {
        led_count = 3;
    } else if (value <= 127) {
        led_count = 4;
    } else if (value <= 159) {
        led_count = 5;
    } else if (value <= 191) {
        led_count = 6;
    } else if (value <= 223) {
        led_count = 7;
    } else {
        led_count = 8;
    }
    
    // 체크박스 업데이트 (왼쪽부터 켜기)
    for(int i = 0; i < 8; i++)
    {
        if(i < led_count)
            pQCheckBox[7-i]->setChecked(true);  // 왼쪽부터
        else
            pQCheckBox[7-i]->setChecked(false);
    }
}

void Tab1DevControl::on_pPBtimerStart_clicked(bool checked)
{
    if(checked)
    {
        QString strValue = ui->pCBtimerValue->currentText();
        pQTimer->start(strValue.toInt());
        ui->pPBtimerStart->setText("TimerStop");
    }
    else
    {
        pQTimer->stop();
        ui->pPBtimerStart->setText("TimerStart");
    }
}

void Tab1DevControl::updateDialValueSlot()
{
    int dialValue = ui->pDialLed->value();
    dialValue++;
    if(dialValue > ui->pDialLed->maximum())
        dialValue = 0;
    ui->pDialLed->setValue(dialValue);
}

void Tab1DevControl::on_pCBtimerValue_currentTextChanged(const QString &arg1)
{
    if(pQTimer->isActive())
    {
        pQTimer->stop();
        pQTimer->start(arg1.toInt());
    }
}

// 서버에서 LED 데이터 받았을 때 (다른 클라이언트가 변경한 경우)
void Tab1DevControl::updateLedFromServer(int value)
{
    qDebug() << "Updating LED from server:" << value;
    
    isUpdatingFromServer = true;  // 플래그 설정
    
    // UI 업데이트
    ui->pDialLed->setValue(value);
    updateProgressBarLedSlot(value);
    
    isUpdatingFromServer = false;  // 플래그 해제
}

QDial* Tab1DevControl::getpDial()
{
    return ui->pDialLed;
}
