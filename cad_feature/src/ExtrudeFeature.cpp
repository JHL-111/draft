#include "cad_feature/ExtrudeFeature.h"
#include "cad_core/CreateBoxCommand.h"
#include <BRepPrimAPI_MakePrism.hxx>
#include <gp_Vec.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <TopoDS_Edge.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#pragma execution_character_set("utf-8")

namespace cad_feature {

ExtrudeFeature::ExtrudeFeature() : Feature(FeatureType::Extrude, "Extrude") {
    SetParameter("distance", 10.0);
    SetParameter("direction_x", 0.0);
    SetParameter("direction_y", 0.0);
    SetParameter("direction_z", 1.0);
    SetParameter("taper_angle", 0.0);
    SetParameter("midplane", 0.0);
}

ExtrudeFeature::ExtrudeFeature(const std::string& name) : Feature(FeatureType::Extrude, name) {
    SetParameter("distance", 10.0);
    SetParameter("direction_x", 0.0);
    SetParameter("direction_y", 0.0);
    SetParameter("direction_z", 1.0);
    SetParameter("taper_angle", 0.0);
    SetParameter("midplane", 0.0);
}

void ExtrudeFeature::SetSketch(const cad_sketch::SketchPtr& sketch) {
    m_sketch = sketch;
}

const cad_sketch::SketchPtr& ExtrudeFeature::GetSketch() const {
    return m_sketch;
}

void ExtrudeFeature::SetDistance(double distance) {
    SetParameter("distance", distance);
}

double ExtrudeFeature::GetDistance() const {
    return GetParameter("distance");
}

void ExtrudeFeature::SetDirection(double x, double y, double z) {
    SetParameter("direction_x", x);
    SetParameter("direction_y", y);
    SetParameter("direction_z", z);
}

void ExtrudeFeature::GetDirection(double& x, double& y, double& z) const {
    x = GetParameter("direction_x");
    y = GetParameter("direction_y");
    z = GetParameter("direction_z");
}

void ExtrudeFeature::SetTaperAngle(double angle) {
    SetParameter("taper_angle", angle);
}

double ExtrudeFeature::GetTaperAngle() const {
    return GetParameter("taper_angle");
}

void ExtrudeFeature::SetMidplane(bool midplane) {
    SetParameter("midplane", midplane ? 1.0 : 0.0);
}

bool ExtrudeFeature::GetMidplane() const {
    return GetParameter("midplane") != 0.0;
}

cad_core::ShapePtr ExtrudeFeature::CreateShape() const {
    if (!ValidateParameters()) {
        return nullptr;
    }
    
    return ExtrudeSketch();
}

bool ExtrudeFeature::ValidateParameters() const {
    if (!IsSketchValid()) {
        return false;
    }
    
    double distance = GetDistance();
    if (distance <= 0.0) {
        return false;
    }
    
    double dx, dy, dz;
    GetDirection(dx, dy, dz);
    double length = std::sqrt(dx*dx + dy*dy + dz*dz);
    if (length < 1e-10) {
        return false;
    }
    
    return true;
}

std::shared_ptr<cad_core::ICommand> ExtrudeFeature::CreateCommand() const {
    // For now, return a simple box command as placeholder
    return std::make_shared<cad_core::CreateBoxCommand>(GetDistance(), GetDistance(), GetDistance());
}

bool ExtrudeFeature::IsSketchValid() const {
    return m_sketch && !m_sketch->IsEmpty();
}

cad_core::ShapePtr ExtrudeFeature::ExtrudeSketch() const {
    if (!IsSketchValid()) {
        return nullptr;
    }

    try {
        // 1. 将草图中的所有线段 (SketchLine) 转换为 OpenCASCADE 的边 (TopoDS_Edge)
        BRepBuilderAPI_MakeWire wireMaker;
        const auto& elements = m_sketch->GetElements();
        for (const auto& elem : elements) {
            if (elem->GetType() == cad_sketch::SketchElementType::Line) {
                auto sketchLine = std::static_pointer_cast<cad_sketch::SketchLine>(elem);
                const auto& startPnt = sketchLine->GetStartPoint()->GetPoint().GetOCCTPoint();
                const auto& endPnt = sketchLine->GetEndPoint()->GetPoint().GetOCCTPoint();

                // 草图坐标是2D的(x,y)，我们需要把它转换回3D空间
                TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(startPnt, endPnt).Edge();
                wireMaker.Add(edge);
            }
        }

        // 2. 从边集合创建成一个封闭的线框 (Wire)
        TopoDS_Wire sketchWire = wireMaker.Wire();
        if (sketchWire.IsNull()) {
            return nullptr; // 如果无法形成线框，则失败
        }

        // 3. 从封闭的线框创建一个面 (Face)
        TopoDS_Face sketchFace = BRepBuilderAPI_MakeFace(sketchWire).Face();
        if (sketchFace.IsNull()) {
            return nullptr; // 如果不是封闭轮廓，则失败
        }

        // 4. 定义拉伸向量
        double distance = GetDistance();
        gp_Vec extrudeVector(0, 0, distance); // 暂时硬编码为沿Z轴拉伸

        // 5. 执行拉伸操作 
        BRepPrimAPI_MakePrism prismMaker(sketchFace, extrudeVector);
        prismMaker.Build();

        if (prismMaker.IsDone()) {
            // 6. 返回拉伸成功后的新三维形状
            return std::make_shared<cad_core::Shape>(prismMaker.Shape());
        }

    }
    catch (const Standard_Failure& e) {
        // 捕获OpenCASCADE的异常
        return nullptr;
    }

    return nullptr;
}

} // namespace cad_feature