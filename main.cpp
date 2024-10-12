
#include "MainWindow.h"
#include <QApplication>
//#include <QOpenGLWidget>

#include "utf8_setup.hpp"

int main(int argc, char* argv[])
{
	QApplication a(argc, argv);

	MainWindow w;
	w.resize(800, 600);
	w.show();

	return a.exec();
}

