#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QWidget>
#include <QTabWidget>
#include "tab1devcontrol.h"
#include "tab2socketclient.h"

class MainWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MainWidget(QWidget *parent = nullptr);
    ~MainWidget();

private:
    QTabWidget *pTabWidget;
    Tab1DevControl *pTab1;
    Tab2SocketClient *pTab2;
};

#endif // MAINWIDGET_H
