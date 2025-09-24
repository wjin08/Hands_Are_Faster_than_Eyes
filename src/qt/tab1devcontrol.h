#ifndef TAB1DEVCONTROL_H
#define TAB1DEVCONTROL_H

#include <QWidget>
#include <QTimer>
#include <QDial>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDebug>

namespace Ui {
class Tab1DevControl;
}

class Tab1DevControl : public QWidget
{
    Q_OBJECT

public:
    explicit Tab1DevControl(QWidget *parent = nullptr);
    ~Tab1DevControl();
    QDial *getpDial();

signals:
    void ledValueChangedSig(int);  // LED 값 변경 시그널

public slots:
    void updateLedFromServer(int);   // 서버에서 LED 데이터 받을 때

private slots:
    void updateProgressBarLedSlot(int);
    void on_pPBtimerStart_clicked(bool checked);
    void updateDialValueSlot();
    void on_pCBtimerValue_currentTextChanged(const QString &arg1);
    void dialValueChangedSlot(int);

private:
    Ui::Tab1DevControl *ui;
    QTimer *pQTimer;
    QButtonGroup *pQButtonGroup;
    QCheckBox *pQCheckBox[8];
    unsigned char lcdData;
    bool isUpdatingFromServer;  // 서버 업데이트 중 플래그
};

#endif // TAB1DEVCONTROL_H
