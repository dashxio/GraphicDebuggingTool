// mainwindow.cpp
#include "mainwindow.h"
#include <BRepPrimAPI_MakeBox.hxx>
#include <AIS_Shape.hxx>
//#include <QtConcurrent>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    mOcctViewer = new OcctViewer(this);
    m_server = new Server(this);
    setCentralWidget(mOcctViewer);

    connect(m_server, &Server::sigDrawDataReady, mOcctViewer, &OcctViewer::drawBrepData);

    m_server->withListenPort("127.0.0.1", "12345")
        .startWork();
}

MainWindow::~MainWindow()
{
}

