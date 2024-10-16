// mainwindow.cpp
#include "server/mainwindow.h"
#include <BRepPrimAPI_MakeBox.hxx>
#include <AIS_Shape.hxx>
//#include <QtConcurrent>
#include <QAction>
#include <QToolBar>
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    QToolBar* tool_bar = addToolBar("Nav");
    QAction* forward_action = new QAction(QIcon::fromTheme("go-next"), "Forward", this);
    QAction* back_action = new QAction(QIcon::fromTheme("go-previous"), "Back", this);
    QAction* toggle_action  = new QAction( "AlwaysDrawNew", this);
    toggle_action->setCheckable(true);

    tool_bar->addAction(back_action);
    tool_bar->addAction(forward_action);
    tool_bar->addAction(toggle_action);

    m_occt_viewer_ = new OcctViewer(this);
    m_server_ = new MyServer(this);
    setCentralWidget(m_occt_viewer_);

    connect(m_server_, &MyServer::sigDrawDataReady, m_occt_viewer_, &OcctViewer::drawBrepData);
    connect(forward_action, &QAction::triggered, m_server_, &MyServer::onMoveNextBrep);
    connect(back_action, &QAction::triggered, m_server_, &MyServer::onMovePreviousBrep);
    connect(toggle_action, &QAction::toggled, m_server_, &MyServer::onUpdateMode);
	connect(toggle_action, &QAction::toggled, [=](bool checked) {
		if (checked) {
			back_action->setEnabled(false);
			forward_action->setEnabled(false);
		}
		else {
			back_action->setEnabled(true);
			forward_action->setEnabled(true);
		}});

    toggle_action->setChecked(true);

    m_server_->withListenPort("127.0.0.1", "12345").run();
}
