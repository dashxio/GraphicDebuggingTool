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
    ~MainWindow() override = default;

private:
    OcctViewer* m_occt_viewer_;
    MyServer* m_server_;
};

#endif // MAINWINDOW_H
