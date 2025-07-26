#include "cad_core/ShapeFactory.h"
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#pragma execution_character_set("utf-8")

namespace cad_core {

ShapePtr ShapeFactory::CreateBox(const Point& corner1, const Point& corner2) {
    try {
        BRepPrimAPI_MakeBox boxMaker(corner1.GetOCCTPoint(), corner2.GetOCCTPoint());
        TopoDS_Shape shape = boxMaker.Shape();
        if (boxMaker.IsDone() && !shape.IsNull()) {
            return std::make_shared<Shape>(shape);
        }
    } catch (...) {
        // 处理OCCT异常
    }
    return nullptr;
}

ShapePtr ShapeFactory::CreateBox(double width, double height, double depth) {
    try {
        // 确保尺寸为正值
        if (width <= 0 || height <= 0 || depth <= 0) {
            return nullptr;
        }
        
        BRepPrimAPI_MakeBox boxMaker(width, height, depth);
        TopoDS_Shape shape = boxMaker.Shape();
        if (boxMaker.IsDone() && !shape.IsNull()) {
            return std::make_shared<Shape>(shape);
        }
    } catch (...) {
        // 处理OCCT异常
    }
    return nullptr;
}

ShapePtr ShapeFactory::CreateCylinder(const Point& center, double radius, double height) {
    try {
        // 确保尺寸为正值
        if (radius <= 0 || height <= 0) {
            return nullptr;
        }
        
        gp_Ax2 axis(center.GetOCCTPoint(), gp_Dir(0, 0, 1));
        BRepPrimAPI_MakeCylinder cylMaker(axis, radius, height);
        TopoDS_Shape shape = cylMaker.Shape();
        if (cylMaker.IsDone() && !shape.IsNull()) {
            return std::make_shared<Shape>(shape);
        }
    } catch (...) {
        // 处理OCCT异常
    }
    return nullptr;
}

ShapePtr ShapeFactory::CreateCylinder(double radius, double height) {
    return CreateCylinder(Point(0, 0, 0), radius, height);
}

ShapePtr ShapeFactory::CreateSphere(const Point& center, double radius) {
    try {
        // 确保半径为正值
        if (radius <= 0) {
            return nullptr;
        }
        
        BRepPrimAPI_MakeSphere sphereMaker(center.GetOCCTPoint(), radius);
        TopoDS_Shape shape = sphereMaker.Shape();
        if (sphereMaker.IsDone() && !shape.IsNull()) {
            return std::make_shared<Shape>(shape);
        }
    } catch (...) {
        // 处理OCCT异常
    }
    return nullptr;
}

ShapePtr ShapeFactory::CreateSphere(double radius) {
    return CreateSphere(Point(0, 0, 0), radius);
}

// 在 ShapeFactory.cpp 中实现
ShapePtr ShapeFactory::CreateTorus(const Point& center,double majorRadius,double minorRadius) {
    try {
        // 确保参数有效
        if (majorRadius <= 0 || minorRadius <= 0) {
            throw std::invalid_argument("圆环半径必须为正值");
        }

        // 使用OpenCASCADE创建圆环
        gp_Ax2 axis(gp_Pnt(center.X(), center.Y(), center.Z()), gp_Dir(0, 0, 1));
        BRepPrimAPI_MakeTorus torusMaker(axis, majorRadius, minorRadius);

        if (!torusMaker.IsDone()) {
            throw std::runtime_error("圆环创建失败");
        }

        return std::make_shared<Shape>(torusMaker.Shape());
    }
    catch (const Standard_Failure& e) {
        // 处理OpenCASCADE异常
        throw std::runtime_error("OpenCASCADE错误: " + std::string(e.GetMessageString()));
    }
}
} 

