// occtviewer.cpp
#include "OCCTViewer.h"
#include <Aspect_DisplayConnection.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Shape.hxx>
#include <AIS_Shape.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>

#ifdef _WIN32
#include <WNT_Window.hxx>
#else
#include <Xw_Window.hxx>
#endif

extern std::string draw_brep_data;

OcctViewer::OcctViewer(QWidget* parent)
    : QWidget(parent)
{
    // 构造函数不进行初始化 OCCT
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_OpaquePaintEvent);
    // 启用鼠标追踪
    setMouseTracking(true);
}

OcctViewer::~OcctViewer()
{
}

void OcctViewer::drawBrepData()
{
	TopoDS_Shape shape;
    std::istringstream iss(draw_brep_data);
    BRep_Builder builder;
    BRepTools::Read(shape, iss, builder);

    if (shape.IsNull()) {
        throw;
    }

    // 显示形状
    Handle(AIS_Shape) aisShape = new AIS_Shape(shape);
    mContext->EraseAll(Standard_False);  // 清除之前的显示
    mContext->Display(aisShape, Standard_True);  // 显示新形状
}

void OcctViewer::initOcctViewer()
{
    // 创建显示连接
    Handle(Aspect_DisplayConnection) aDisplayConnection = new Aspect_DisplayConnection();

    // 创建 OpenGl 图形驱动
    Handle(Graphic3d_GraphicDriver) aGraphicDriver = new OpenGl_GraphicDriver(aDisplayConnection);

    // 创建 Viewer
    mViewer = new V3d_Viewer(aGraphicDriver);
    mViewer->SetDefaultLights();
    mViewer->SetLightOn();

    // 创建 View
    mView = mViewer->CreateView();
    mView->SetBackgroundColor(Quantity_NOC_BLACK);

    // 创建交互上下文
    mContext = new AIS_InteractiveContext(mViewer);

    // 绑定窗口
    WId windowHandle = winId();

#ifdef _WIN32
    Handle(WNT_Window) wind = new WNT_Window((Aspect_Handle)windowHandle);
#else
    Handle(Xw_Window) wind = new Xw_Window(aDisplayConnection, (Window)windowHandle);
#endif

    mView->SetWindow(wind);

    if (!wind->IsMapped())
    {
        wind->Map();
    }

    // 初始化视图
    mView->MustBeResized();
    mView->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_WHITE, 0.08, V3d_ZBUFFER);

    // 发出初始化完成的信号
    emit initialized();
}

void OcctViewer::showEvent(QShowEvent* event)
{
    // 在窗口显示时初始化 OCCT 视图
    initOcctViewer();
    QWidget::showEvent(event);
}

void OcctViewer::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    if (!mView.IsNull())
    {
        mView->Redraw();
    }
}

void OcctViewer::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    if (!mView.IsNull())
    {
        mView->MustBeResized();
    }
}

void OcctViewer::mousePressEvent(QMouseEvent* event)
{
    if (mView.IsNull() || mContext.IsNull())
        return;

    // 获取鼠标位置
    int x = event->x();
    int y = height() - event->y();
    mLastMousePos = event->pos();

    if (event->button() == Qt::LeftButton) {
        // 旋转视图
        mView->StartRotation(x, y);
    }
}

void OcctViewer::mouseReleaseEvent(QMouseEvent* event)
{
    if (mView.IsNull() || mContext.IsNull())
        return;

    // 鼠标释放，结束旋转操作
    if (event->button() == Qt::LeftButton) {
        // 停止旋转操作时不需要特殊处理
    }
}

void OcctViewer::mouseMoveEvent(QMouseEvent* event)
{
    if (mView.IsNull() || mContext.IsNull())
        return;

    int x = event->x();
    int y = height() - event->y();

    // 左键按下时旋转视图
    if (event->buttons() & Qt::LeftButton) {
        mView->Rotation(x, y);
    }
    else if (event->buttons() & Qt::MiddleButton) {
        // 平移视图
        QPoint delta = event->pos() - mLastMousePos;
        mView->Pan(delta.x(), -delta.y());
    }
    else if (event->buttons() & Qt::RightButton) {
        // 右键拖动缩放视图
        QPoint delta = event->pos() - mLastMousePos;
        double zoomFactor = 1.0 + (delta.y() / 100.0);
        mView->SetZoom(zoomFactor);
    }

    mLastMousePos = event->pos();
    mView->Redraw();
}

void OcctViewer::wheelEvent(QWheelEvent* event)
{
    if (mView.IsNull() || mContext.IsNull())
        return;

    int delta = event->angleDelta().y();
    if (delta != 0) {
        Standard_Real factor = (delta > 0) ? 1.1 : 0.9;
        mView->SetZoom(factor);
    }

    mView->Redraw();
}



