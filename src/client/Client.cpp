#pragma once

//        gp_Pnt p1(0.0, 0.0, 0.0);
//        gp_Pnt p2(10.0, 10.0, 10.0);
//        Handle(Geom_Line) lineGeom = new Geom_Line(p1, gp_Dir(p2.XYZ()));
#include "client/RemoteDebugTools.hpp"

int main() {
    Client c;
    c.connectServer("127.0.0.1", 12345);

    // 创建几何对象
    TopoDS_Shape shape1 = createLine({ 0,0,0 }, { 100,100,100 });
    TopoDS_Shape shape2 = createBox(100);

    // 将几何对象序列化为 BRep 数据
    std::string brepData = shapeToBRep(shape1);
    c.sendBrepData(brepData);

    brepData = shapeToBRep(shape2);
    c.sendBrepData(brepData);

    int a;
    std::cin >> a;
    return 0;
}