// mainwindow.h
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "OCCTViewer.h"
#include "Server.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private:
    OcctViewer* mOcctViewer;
    MyServer* m_server;
};

#endif // MAINWINDOW_H
