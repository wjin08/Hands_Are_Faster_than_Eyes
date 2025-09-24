#ifndef TAB2SOCKETCLIENT_H
#define TAB2SOCKETCLIENT_H

#include <QWidget>
#include <QDebug>
#include <QTime>
#include "socketclient.h"

namespace Ui {
class Tab2SocketClient;
}

class Tab2SocketClient : public QWidget
{
    Q_OBJECT

public:
    explicit Tab2SocketClient(QWidget *parent = nullptr);
    ~Tab2SocketClient();
    SocketClient * getpSocketClient();

signals:
    void ledWriteSig(int);    // LED 데이터 수신 시그널

private slots:
    void on_pPBserverConnect_toggled(bool checked);
    void updateRecvDataSlot(QString);
    void on_pPBrecvDataClear_clicked();
    void on_pPBSend_clicked();

public slots:
    void socketSendLedData(int);      // LED 데이터 전송

private:
    Ui::Tab2SocketClient *ui;
    SocketClient *pSocketClient;
    QString lastSentLedValue;  // 마지막으로 보낸 LED 값 저장
};

#endif // TAB2SOCKETCLIENT_H
