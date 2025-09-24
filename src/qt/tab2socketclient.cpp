#include "tab2socketclient.h"
#include "ui_tab2socketclient.h"

Tab2SocketClient::Tab2SocketClient(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::Tab2SocketClient)
{
    ui->setupUi(this);
    ui->pPBSend->setEnabled(false);

    pSocketClient = new SocketClient(this);
    connect(pSocketClient, SIGNAL(socketRecvDataSig(QString)), 
            this, SLOT(updateRecvDataSlot(QString)));
}

Tab2SocketClient::~Tab2SocketClient()
{
    delete ui;
}

void Tab2SocketClient::on_pPBserverConnect_toggled(bool checked)
{
    bool bFlag;
    if (checked)
    {
        pSocketClient->connectToServerSlot(bFlag);
        if (bFlag)
        {
            ui->pPBserverConnect->setText("서버 해제");
            ui->pPBSend->setEnabled(true);
        }
        else
        {
            ui->pPBserverConnect->setChecked(false);
        }
    }
    else
    {
        pSocketClient->socketClosedServerSlot();
        ui->pPBserverConnect->setText("서버 연결");
        ui->pPBSend->setEnabled(false);
    }
}

void Tab2SocketClient::updateRecvDataSlot(QString strRecvData)
{
    strRecvData.chop(1);   // 끝문자 "\n" 제거
    QTime time = QTime::currentTime();
    QString strTime = time.toString("hh:mm:ss");
    
    // 모든 수신 데이터를 로그에 표시
    QString logMessage = strTime + " | " + strRecvData;
    ui->pTErecvData->append(logMessage);
    
    // LED 업데이트 처리 (서버에서 보낸 LED_UPDATE)
    if (strRecvData.contains("[SERVER]LED_UPDATE@"))
    {
        int atPos = strRecvData.indexOf("LED_UPDATE@");
        if (atPos != -1)
        {
            QString ledStr = strRecvData.mid(atPos + 11);  // "LED_UPDATE@" 다음
            bool ok;
            int ledValue = 0;
            
            if (ledStr.startsWith("0x", Qt::CaseInsensitive))
            {
                ledValue = ledStr.toInt(&ok, 16);
            }
            else
            {
                ledValue = ledStr.toInt(&ok);
            }
            
            if (ok)
            {
                emit ledWriteSig(ledValue);
                ui->pTErecvData->append(QString("  → LED Updated to: %1 (0x%2)")
                    .arg(ledValue)
                    .arg(ledValue, 2, 16, QChar('0')).toUpper());
            }
        }
    }
    // 다른 클라이언트의 LED 명령
    else if (strRecvData.contains("LED@"))
    {
        // 다른 클라이언트가 보낸 LED 명령도 UI 업데이트
        int atPos = strRecvData.indexOf("LED@");
        if (atPos != -1)
        {
            QString ledStr = strRecvData.mid(atPos + 4);
            bool ok;
            int ledValue = 0;
            
            if (ledStr.startsWith("0x", Qt::CaseInsensitive))
            {
                ledValue = ledStr.toInt(&ok, 16);
            }
            else
            {
                ledValue = ledStr.toInt(&ok);
            }
            
            if (ok && !strRecvData.contains("[KSH_QT]"))  // 자신이 보낸 것이 아닌 경우
            {
                emit ledWriteSig(ledValue);
                ui->pTErecvData->append(QString("  → Other client LED: %1")
                    .arg(ledValue));
            }
        }
    }
    
    // 스크롤을 최하단으로 이동
    QTextCursor cursor = ui->pTErecvData->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui->pTErecvData->setTextCursor(cursor);
}

void Tab2SocketClient::on_pPBrecvDataClear_clicked()
{
    ui->pTErecvData->clear();
}

void Tab2SocketClient::on_pPBSend_clicked()
{
    QString strRecvId = ui->pLErecvId->text();
    QString strSendData = ui->pLEsendData->text();

    if (strRecvId.isEmpty())
    {
        strSendData = "[ALLMSG]" + strSendData;
    }
    else
    {
        strSendData = "[" + strRecvId + "]" + strSendData;
    }

    pSocketClient->socketWriteDataSlot(strSendData);
    
    // 송신 로그 추가
    QTime time = QTime::currentTime();
    QString strTime = time.toString("hh:mm:ss");
    ui->pTErecvData->append(strTime + " | [SENT] " + strSendData);
    
    ui->pLEsendData->clear();
}

// Tab1에서 LED 값 변경시 호출
void Tab2SocketClient::socketSendLedData(int ledNo)
{
    QString data = QString("[KSH_QT]LED@0x%1").arg(ledNo, 2, 16, QChar('0'));
    pSocketClient->socketWriteDataSlot(data);
    
    // 송신 로그 추가
    QTime time = QTime::currentTime();
    QString strTime = time.toString("hh:mm:ss");
    ui->pTErecvData->append(strTime + " | [SENT] " + data);
    
    qDebug() << "Sending LED data:" << data;
}

SocketClient* Tab2SocketClient::getpSocketClient()
{
    return pSocketClient;
}
