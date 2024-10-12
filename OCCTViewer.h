// occtviewer.h
#ifndef OCCTVIEWER_H
#define OCCTVIEWER_H

#include <QWidget>
#include <QMouseEvent>
#include <AIS_InteractiveContext.hxx>
#include <V3d_View.hxx>

class OcctViewer : public QWidget
{
    Q_OBJECT
public:
    explicit OcctViewer(QWidget* parent = nullptr);
    ~OcctViewer();

    Handle(AIS_InteractiveContext) getContext() const { return mContext; }
    void drawBrepData();

signals:
    void initialized();

protected:
    // 重写 QWidget 的事件处理方法
    void initOcctViewer();
    void showEvent(QShowEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    // 事件处理
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

    QPaintEngine* paintEngine() const override {
        return nullptr;// 返回nullptr，告诉 Qt 不使用它自己的绘图引擎
    }
private:
    Handle(V3d_Viewer) mViewer;
    Handle(V3d_View) mView;
    Handle(AIS_InteractiveContext) mContext;

    QPoint mLastMousePos;
};

#endif // OCCTVIEWER_H
