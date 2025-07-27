#include "cad_ui/SketchMode.h"
#include "cad_ui/QtOccView.h"
#include "cad_sketch/SketchPoint.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDebug>
#include <BRep_Tool.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <V3d_View.hxx>
#include <cmath>
#pragma execution_character_set("utf-8")
#include <GeomAPI_IntCS.hxx>          
#include <AIS_Shape.hxx>
#include <AIS_InteractiveContext.hxx>
#include <ElSLib.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_LineAspect.hxx>
#include <Quantity_Color.hxx>
#include <Aspect_TypeOfLine.hxx>
#include <Graphic3d_Camera.hxx>

namespace cad_ui {

// =============================================================================
// SketchRectangleTool Implementation
// =============================================================================

SketchRectangleTool::SketchRectangleTool(QObject* parent)
    : QObject(parent), m_isDrawing(false) {
}

void SketchRectangleTool::StartDrawing(const QPoint& startPoint) {
    m_isDrawing = true;
    m_startPoint = startPoint;
    m_currentPoint = startPoint;
    m_currentLines.clear();
    
    qDebug() << "Rectangle tool: Started drawing at" << startPoint;
}

void SketchRectangleTool::UpdateDrawing(const QPoint& currentPoint) {
    if (!m_isDrawing) {
        return;
    }

    m_currentPoint = currentPoint;

    qDebug() << "UpdateDrawing: Start(" << m_startPoint.x() << "," << m_startPoint.y()
        << ") Current(" << currentPoint.x() << "," << currentPoint.y() << ")";

    // 创建临时矩形线条用于预览
    gp_Pnt startPnt = ScreenToSketchPlane(m_startPoint);
    gp_Pnt currentPnt = ScreenToSketchPlane(m_currentPoint);

    qDebug() << "Converted points: Start(" << startPnt.X() << "," << startPnt.Y()
        << ") Current(" << currentPnt.X() << "," << currentPnt.Y() << ")";

    // 检查点是否有效
    if (std::abs(currentPnt.X() - startPnt.X()) < 0.1 && std::abs(currentPnt.Y() - startPnt.Y()) < 0.1) {
        qDebug() << "Points too close, skipping preview update";
        return;
    }

    m_currentLines = CreateRectangleLines(startPnt, currentPnt);

    qDebug() << "Created" << m_currentLines.size() << "preview lines";

    emit previewUpdated(m_currentLines);
}

void SketchRectangleTool::FinishDrawing(const QPoint& endPoint) {
    if (!m_isDrawing) {
        return;
    }
    
    m_currentPoint = endPoint;
    
    // 创建最终的矩形
    gp_Pnt startPnt = ScreenToSketchPlane(m_startPoint);
    gp_Pnt endPnt = ScreenToSketchPlane(m_currentPoint);
    
    auto rectangleLines = CreateRectangleLines(startPnt, endPnt);
    
    m_isDrawing = false;
    m_currentLines.clear();
    
    emit rectangleCreated(rectangleLines);
    
    qDebug() << "Rectangle tool: Finished drawing rectangle with" << rectangleLines.size() << "lines";
}

void SketchRectangleTool::CancelDrawing() {
    if (!m_isDrawing) {
        return;
    }
    
    m_isDrawing = false;
    m_currentLines.clear();
    
    emit drawingCancelled();
    
    qDebug() << "Rectangle tool: Drawing cancelled";
}

void SketchRectangleTool::SetSketchPlane(const gp_Pln& plane) {
    m_sketchPlane = plane;
}

void SketchRectangleTool::SetView(Handle(V3d_View) view) {
    m_view = view;
}

std::vector<cad_sketch::SketchLinePtr> SketchRectangleTool::GetCurrentRectangle() const {
    return m_currentLines;
}

gp_Pnt SketchRectangleTool::ScreenToSketchPlane(const QPoint& screenPoint) {
    if (m_view.IsNull()) {
        qDebug() << "View is null in ScreenToSketchPlane";
        return gp_Pnt(0, 0, 0);
    }

    // 使用射线投影到平面
    Standard_Real Xp, Yp, Zp, Xv, Yv, Zv;
    m_view->Convert(screenPoint.x(), screenPoint.y(), Xp, Yp, Zp);
    m_view->Proj(Xv, Yv, Zv);

    gp_Lin line(gp_Pnt(Xp, Yp, Zp), gp_Dir(Xv, Yv, Zv));

    // 计算射线与平面的交点
    Handle(Geom_Plane) plane = new Geom_Plane(m_sketchPlane);
    GeomAPI_IntCS intCS;
    intCS.Perform(new Geom_Line(line), plane);

    if (intCS.IsDone() && intCS.NbPoints() > 0) {
        gp_Pnt result = intCS.Point(1);

        // 将3D点转换到草图平面的2D坐标系
        Standard_Real u, v;
        ElSLib::Parameters(m_sketchPlane, result, u, v);

        qDebug() << "ScreenToSketchPlane: Screen(" << screenPoint.x() << "," << screenPoint.y()
            << ") -> Plane(" << u << "," << v << ")";

        // 返回草图平面2D坐标系中的点
        return gp_Pnt(u, v, 0);
    }

    qDebug() << "Failed to intersect ray with plane";
    return gp_Pnt(0, 0, 0);
}

std::vector<cad_sketch::SketchLinePtr> SketchRectangleTool::CreateRectangleLines(
    const gp_Pnt& point1, const gp_Pnt& point2) {
    
    std::vector<cad_sketch::SketchLinePtr> lines;
    
    // 创建矩形的四个角点
    auto bottomLeft = std::make_shared<cad_sketch::SketchPoint>(
        std::min(point1.X(), point2.X()), 
        std::min(point1.Y(), point2.Y()));
    auto bottomRight = std::make_shared<cad_sketch::SketchPoint>(
        std::max(point1.X(), point2.X()), 
        std::min(point1.Y(), point2.Y()));
    auto topRight = std::make_shared<cad_sketch::SketchPoint>(
        std::max(point1.X(), point2.X()), 
        std::max(point1.Y(), point2.Y()));
    auto topLeft = std::make_shared<cad_sketch::SketchPoint>(
        std::min(point1.X(), point2.X()), 
        std::max(point1.Y(), point2.Y()));
    
    // 创建四条边
    lines.push_back(std::make_shared<cad_sketch::SketchLine>(bottomLeft, bottomRight)); // 底边
    lines.push_back(std::make_shared<cad_sketch::SketchLine>(bottomRight, topRight));   // 右边
    lines.push_back(std::make_shared<cad_sketch::SketchLine>(topRight, topLeft));       // 顶边
    lines.push_back(std::make_shared<cad_sketch::SketchLine>(topLeft, bottomLeft));     // 左边
    
    return lines;
}


// =============================================================================
SketchLineTool::SketchLineTool(QObject* parent)
    : QObject(parent), m_isDrawing(false) {
    m_sketchPlane = gp_Pln(gp_Pnt(1.0e+100, 0, 0), gp_Dir(0, 0, 1));
}

void SketchLineTool::StartDrawing(const QPoint& startPoint) {
    m_startPoint3d = ScreenToSketchPlane(startPoint);
    m_isDrawing = true;
}

void SketchLineTool::UpdateDrawing(const QPoint& currentPoint) {
    if (!m_isDrawing) return;
    gp_Pnt currentPoint3d = ScreenToSketchPlane(currentPoint);

    auto start = std::make_shared<cad_sketch::SketchPoint>(m_startPoint3d.X(), m_startPoint3d.Y());
    auto end = std::make_shared<cad_sketch::SketchPoint>(currentPoint3d.X(), currentPoint3d.Y());
    auto previewLine = std::make_shared<cad_sketch::SketchLine>(start, end);

    emit previewUpdated({ previewLine });
}

void SketchLineTool::FinishDrawing(const QPoint& endPoint) {
    if (!m_isDrawing) return;
    m_isDrawing = false;

    gp_Pnt endPoint3d = ScreenToSketchPlane(endPoint);

    auto start = std::make_shared<cad_sketch::SketchPoint>(m_startPoint3d.X(), m_startPoint3d.Y());
    auto end = std::make_shared<cad_sketch::SketchPoint>(endPoint3d.X(), endPoint3d.Y());

    if (start->GetPoint().Distance(end->GetPoint()) > 1e-6) { // 避免创建零长度线
        emit lineCreated(std::make_shared<cad_sketch::SketchLine>(start, end));
    }
    else {
        emit drawingCancelled();
    }
}

void SketchLineTool::CancelDrawing() {
    if (!m_isDrawing) return;
    m_isDrawing = false;
    emit drawingCancelled();
}

void SketchLineTool::SetSketchPlane(const gp_Pln& plane) {
    m_sketchPlane = plane;
}

void SketchLineTool::SetView(Handle(V3d_View) view) {
    m_view = view;
}

gp_Pnt SketchLineTool::ScreenToSketchPlane(const QPoint& screenPoint) {
    if (m_view.IsNull() || m_sketchPlane.Location().X() > 1.0e+99) {
        return gp_Pnt(0, 0, 0);
    }
    Standard_Real aGridX, aGridY, aGridZ;
    m_view->ConvertToGrid(screenPoint.x(), screenPoint.y(), aGridX, aGridY, aGridZ);
    gp_Pnt aPntOnPlane(aGridX, aGridY, aGridZ);
    Standard_Real u, v;
    ElSLib::Parameters(m_sketchPlane, aPntOnPlane, u, v);
    gp_Pnt2d pnt2d(u, v);
    return gp_Pnt(pnt2d.X(), pnt2d.Y(), 0);
}

// =============================================================================
// ^^^^^^^^^^^^^^  SketchLineTool Implementation End ^^^^^^^^^^^^^^
// =============================================================================


// =============================================================================
// SketchMode Implementation
// =============================================================================

SketchMode::SketchMode(QtOccView* viewer, QObject* parent)
    : QObject(parent), m_viewer(viewer), m_isActive(false), m_activeTool(ActiveTool::None){
    
    // 创建绘制工具
    m_rectangleTool = std::make_unique<SketchRectangleTool>(this);
	m_lineTool = std::make_unique<SketchLineTool>(this);

    // 连接信号槽
    connect(m_rectangleTool.get(), &SketchRectangleTool::rectangleCreated, this, &SketchMode::OnRectangleCreated);
    connect(m_rectangleTool.get(), &SketchRectangleTool::drawingCancelled, this, &SketchMode::OnDrawingCancelled);
    connect(m_rectangleTool.get(), &SketchRectangleTool::previewUpdated, this, &SketchMode::UpdatePreview);

    connect(m_lineTool.get(), &SketchLineTool::lineCreated, this, &SketchMode::OnLineCreated);
    connect(m_lineTool.get(), &SketchLineTool::previewUpdated, this, &SketchMode::UpdatePreview);
    connect(m_lineTool.get(), &SketchLineTool::drawingCancelled, this, &SketchMode::OnDrawingCancelled);
}

bool SketchMode::EnterSketchMode(const TopoDS_Face& face) {
    if (m_isActive) {
        qDebug() << "Already in sketch mode, exiting first";
        ExitSketchMode();
    }
    
    // 检查参数有效性
    if (face.IsNull()) {
        qDebug() << "Error: Cannot enter sketch mode with null face";
        return false;
    }
    
    if (!m_viewer) {
        qDebug() << "Error: No viewer available";
        return false;
    }
    
    try {
        // 保存当前视图状态
        if (!m_viewer->GetView().IsNull()) {
            Handle(Graphic3d_Camera) camera = m_viewer->GetView()->Camera();
            if (!camera.IsNull()) {
                m_savedEye = camera->Eye();
                m_savedAt = camera->Center();
                m_savedUp = camera->Up();
                m_savedScale = camera->Scale();
            } else {
                qDebug() << "Warning: Camera is null, using default values";
                m_savedEye = gp_Pnt(0, 0, 100);
                m_savedAt = gp_Pnt(0, 0, 0);
                m_savedUp = gp_Dir(0, 1, 0);
                m_savedScale = 1.0;
            }
        } else {
            qDebug() << "Warning: View is null, using default values";
            m_savedEye = gp_Pnt(0, 0, 100);
            m_savedAt = gp_Pnt(0, 0, 0);
            m_savedUp = gp_Dir(0, 1, 0);
            m_savedScale = 1.0;
        }
        
        // 设置草图信息
        m_sketchFace = face;
        SetupSketchPlane(face);
        
        // 创建新的草图
        m_currentSketch = std::make_shared<cad_sketch::Sketch>("Sketch_001");
        
        // 设置草图视图
        SetupSketchView();
        
        // 设置绘制工具
        m_rectangleTool->SetSketchPlane(m_sketchPlane);
        m_rectangleTool->SetView(m_viewer->GetView());
		m_lineTool->SetSketchPlane(m_sketchPlane);
		m_lineTool->SetView(m_viewer->GetView());

        m_isActive = true;
        
        emit sketchModeEntered();
        emit statusMessageChanged("进入草图模式 - 点击\"矩形\"工具开始绘制");
        
        qDebug() << "Entered sketch mode successfully";
        return true;
    }
    catch (const std::exception& e) {
        qDebug() << "Failed to enter sketch mode:" << e.what();
        return false;
    }
}

void SketchMode::ExitSketchMode() {
    if (!m_isActive) {
        return;
    }
    
    // 停止当前工具
    StopCurrentTool();
    
    // 恢复视图
    RestoreView();
    
	// 清除草图显示
	ClearAllSketchDisplay();

    // 清理草图数据
    m_currentSketch.reset();
    m_sketchFace = TopoDS_Face();
    
    m_isActive = false;
    
    emit sketchModeExited();
    emit statusMessageChanged("退出草图模式");
    
    qDebug() << "Exited sketch mode";
}

void SketchMode::StartRectangleTool() {
    if (!m_isActive) {
        return;
    }
    
    StopCurrentTool();
	m_activeTool = ActiveTool::Rectangle;
    emit statusMessageChanged("矩形工具 - 点击并拖拽创建矩形");
    
    qDebug() << "Started rectangle tool";
}

void SketchMode::StartLineTool() {
    if (!m_isActive) {
        return;
    }
   
    StopCurrentTool();
    m_activeTool = ActiveTool::Line; 
    emit statusMessageChanged("直线工具 - 点击并拖拽创建直线");
   
    qDebug() << "Started line tool";
}

void SketchMode::StopCurrentTool() {
    if (m_rectangleTool && m_rectangleTool->IsDrawing()) {
        m_rectangleTool->CancelDrawing();
    }
    if (m_lineTool && m_lineTool->IsDrawing()) {
        m_lineTool->CancelDrawing();
		m_activeTool = ActiveTool::None; 
    }
}

void SketchMode::HandleMousePress(QMouseEvent* event) {
    if (!m_isActive || event->button() != Qt::LeftButton) return;

    if (m_activeTool == ActiveTool::Rectangle) {
        m_rectangleTool->StartDrawing(event->pos());
    }
    else if (m_activeTool == ActiveTool::Line) { 
        m_lineTool->StartDrawing(event->pos());
    }
}

void SketchMode::HandleMouseMove(QMouseEvent* event) {
    if (!m_isActive) return;

    if (m_activeTool == ActiveTool::Rectangle) {
        m_rectangleTool->UpdateDrawing(event->pos());
    }
    else if (m_activeTool == ActiveTool::Line) { 
        m_lineTool->UpdateDrawing(event->pos());
    }
}

void SketchMode::HandleMouseRelease(QMouseEvent* event) {
    if (!m_isActive || event->button() != Qt::LeftButton) return;

    if (m_activeTool == ActiveTool::Rectangle) {
        m_rectangleTool->FinishDrawing(event->pos());
    }
    else if (m_activeTool == ActiveTool::Line) { 
        m_lineTool->FinishDrawing(event->pos());
    }
}

void SketchMode::HandleKeyPress(QKeyEvent* event) {
    if (!m_isActive) {
        return;
    }
    
    if (event->key() == Qt::Key_Escape) {
        if (m_rectangleTool->IsDrawing()) {
            m_rectangleTool->CancelDrawing();
        } else {
            ExitSketchMode();
        }
    }
}

void SketchMode::OnRectangleCreated(const std::vector<cad_sketch::SketchLinePtr>& lines) {
    if (!m_currentSketch) {
        return;
    }
    
	ClearPreviewDisplay(); // 清除预览图形    

    // 将线条添加到草图中
    for (const auto& line : lines) {
        m_currentSketch->AddElement(line);
		DisplaySketchElement(line); // 显示草图元素   
        emit sketchElementCreated(line);
    }
    
    emit statusMessageChanged(QString("创建了矩形，包含 %1 条线").arg(lines.size()));
    
    qDebug() << "Added rectangle with" << lines.size() << "lines to sketch";
}

void SketchMode::OnLineCreated(const cad_sketch::SketchLinePtr& line) {
    if (!m_currentSketch) {
        return;
    }

    ClearPreviewDisplay();

    m_currentSketch->AddElement(line);
    DisplaySketchElement(line);
    emit sketchElementCreated(line);

    emit statusMessageChanged("创建了直线");
}

void SketchMode::OnDrawingCancelled() {
	ClearPreviewDisplay(); // 清除预览图形    
    emit statusMessageChanged("绘制已取消");
}

void SketchMode::SetupSketchPlane(const TopoDS_Face& face) {
    m_sketchPlane = ExtractPlaneFromFace(face);
    CreateSketchCoordinateSystem();
}

void SketchMode::SetupSketchView() {
    if (!m_viewer || m_viewer->GetView().IsNull()) {
        qDebug() << "Warning: Cannot setup sketch view - viewer or view is null";
        return;
    }

    try {
        Handle(V3d_View) view = m_viewer->GetView();
        Handle(Graphic3d_Camera) camera = view->Camera();

        if (camera.IsNull()) {
            qDebug() << "Warning: Camera is null in SetupSketchView";
            return;
        }

        // 获取草图平面的法向量和位置
        gp_Pnt planeOrigin = m_sketchPlane.Location();
        gp_Dir planeNormal = m_sketchPlane.Axis().Direction();

        // 设置视图方向（正对草图平面）
        // 将相机位置设置在平面前方一定距离
        double viewDistance = 500.0; // 增加视距以确保能看到整个草图
        gp_Pnt eyePosition = planeOrigin.XYZ() + planeNormal.XYZ() * viewDistance;

        // 设置相机参数
        camera->SetEye(eyePosition);
        camera->SetCenter(planeOrigin);

        // 设置向上方向 - 使用草图坐标系的Y方向
        gp_Dir yDir = m_sketchCS.YDirection();
        camera->SetUp(yDir);

        // 设置正交投影（对草图更合适）
        camera->SetProjectionType(Graphic3d_Camera::Projection_Orthographic);

        // 设置合适的缩放比例
        double scale = 100.0; // 根据需要调整
        camera->SetScale(scale);

        // 调整视图大小
        view->FitAll(0.01, Standard_False);
        view->ZFitAll();

        // 强制重绘
        view->Redraw();

        qDebug() << "Setup sketch view completed:";
        qDebug() << "  Eye:" << eyePosition.X() << eyePosition.Y() << eyePosition.Z();
        qDebug() << "  Center:" << planeOrigin.X() << planeOrigin.Y() << planeOrigin.Z();
        qDebug() << "  Normal:" << planeNormal.X() << planeNormal.Y() << planeNormal.Z();
        qDebug() << "  Scale:" << scale;
    }
    catch (const std::exception& e) {
        qDebug() << "Error in SetupSketchView:" << e.what();
    }
}

void SketchMode::RestoreView() {
    if (m_viewer->GetView().IsNull()) {
        return;
    }
    
    Handle(V3d_View) view = m_viewer->GetView();
    
    // 恢复透视投影
    view->Camera()->SetProjectionType(Graphic3d_Camera::Projection_Perspective);
    
    // 恢复保存的视图状态
    view->Camera()->SetEye(m_savedEye);
    view->Camera()->SetCenter(m_savedAt);
    view->Camera()->SetUp(m_savedUp);
    view->Camera()->SetScale(m_savedScale);
    
    qDebug() << "Restored view";
}

void SketchMode::CreateSketchCoordinateSystem() {
    try {
        // 基于草图平面创建坐标系
        gp_Pnt origin = m_sketchPlane.Location();
        gp_Dir zAxis = m_sketchPlane.Axis().Direction();
        gp_Dir xAxis = m_sketchPlane.XAxis().Direction();
        
        // 验证方向向量有效性 (gp_Dir已经是单位向量，检查是否为有效方向)
        try {
            // gp_Dir构造时会自动标准化，这里简单验证即可
            gp_Dir testZ = zAxis;
            gp_Dir testX = xAxis;
        } catch (...) {
            qDebug() << "Warning: Invalid axis directions, using default coordinate system";
            m_sketchCS = gp_Ax3(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0));
            return;
        }
        
        m_sketchCS = gp_Ax3(origin, zAxis, xAxis);
        qDebug() << "Sketch coordinate system created successfully";
    }
    catch (const std::exception& e) {
        qDebug() << "Error creating sketch coordinate system:" << e.what();
        // 使用默认坐标系
        m_sketchCS = gp_Ax3(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0));
    }
}

gp_Pln SketchMode::ExtractPlaneFromFace(const TopoDS_Face& face) {
    try {
        if (face.IsNull()) {
            qDebug() << "Error: Face is null, using default XY plane";
            return gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));
        }
        
        Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
        if (surface.IsNull()) {
            qDebug() << "Error: Surface is null, using default XY plane";
            return gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));
        }
        
        Handle(Geom_Plane) plane = Handle(Geom_Plane)::DownCast(surface);
        
        if (!plane.IsNull()) {
            gp_Pln result = plane->Pln();
            qDebug() << "Successfully extracted plane from face";
            return result;
        }
        
        // 如果不是平面，创建一个默认的XY平面
        qDebug() << "Warning: Selected face is not a plane, using XY plane";
        return gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));
    }
    catch (const std::exception& e) {
        qDebug() << "Error extracting plane from face:" << e.what();
        return gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));
    }
}

// 将 SketchLine 转换为 TopoDS_Edge
TopoDS_Edge SketchMode::ConvertLineToEdge(const cad_sketch::SketchLinePtr& line) const {
    const auto& p1 = line->GetStartPoint()->GetPoint().GetOCCTPoint();
    const auto& p2 = line->GetEndPoint()->GetPoint().GetOCCTPoint();
    
    if (p1.IsEqual(p2, 1e-9)) {
        return TopoDS_Edge(); // 如果点重合，返回一个空的无效Edge，阻止崩溃
    }
    
    return BRepBuilderAPI_MakeEdge(p1, p2).Edge();
}

// 清除预览图形
void SketchMode::ClearPreviewDisplay() {
    if (!m_viewer || m_viewer->GetContext().IsNull()) return;
    for (const auto& shape : m_previewElements) {
        m_viewer->GetContext()->Remove(shape, Standard_False); // 移除但不立即重绘
    }
    m_previewElements.clear();

    // 通知查看器内容已更新
    m_viewer->GetContext()->UpdateCurrentViewer();
}

// 更新预览图形
void SketchMode::UpdatePreview(const std::vector<cad_sketch::SketchLinePtr>& previewLines) {
    if (!m_viewer || m_viewer->GetContext().IsNull()) return;

    ClearPreviewDisplay(); // 先清除旧的预览

    for (const auto& line : previewLines) {
        TopoDS_Edge edge = ConvertLineToEdge(line);
        if (edge.IsNull()) {
            continue;
        }
        auto aisShape = new AIS_Shape(edge);
        aisShape->Attributes()->SetLineAspect(new Prs3d_LineAspect(Quantity_NOC_BLUE1, Aspect_TOL_DOT, 2.0));

        // 添加Z层设置，解决Z-Fighting问题
        aisShape->SetZLayer(Graphic3d_ZLayerId_Topmost);

        m_previewElements.push_back(aisShape);
        m_viewer->GetContext()->Display(aisShape, Standard_False); // 显示但不立即重绘
    }

    // 在所有对象都添加完毕后，手动触发一次重绘
    m_viewer->GetView()->Redraw();
}

// 显示最终的草图元素
void SketchMode::DisplaySketchElement(const cad_sketch::SketchElementPtr& element) {
    // 安全检查，确保视图和元素都有效
    if (!m_viewer || m_viewer->GetContext().IsNull() || !element) {
        return;
    }
    // 处理不同类型的草图元素（目前只有直线）
    if (element->GetType() == cad_sketch::SketchElementType::Line) {
        auto line = std::dynamic_pointer_cast<cad_sketch::SketchLine>(element);

        // 将逻辑线段转换为OpenCASCADE的边
        TopoDS_Edge edge = ConvertLineToEdge(line);

        auto aisShape = new AIS_Shape(edge);
        aisShape->Attributes()->SetLineAspect(new Prs3d_LineAspect(Quantity_NOC_BLACK, Aspect_TOL_SOLID, 2.0));

        // 设置Z层，解决被模型表面遮挡（Z-Fighting）的问题
        aisShape->SetZLayer(Graphic3d_ZLayerId_Topmost);

        // 将草图元素与其对应的显示对象关联起来，方便后续管理
        m_displayedElements[element] = aisShape;

        // 将显示对象添加到3D场景中，但不立即重绘（Standard_False）
        m_viewer->GetContext()->Display(aisShape, Standard_False);
    }
    // 未来可以在这里添加对 Circle, Arc 等其他类型的显示支持\

    m_viewer->GetContext()->UpdateCurrentViewer();
    m_viewer->GetView()->Redraw();
}

// 清除所有草图显示（退出时使用）
void SketchMode::ClearAllSketchDisplay() {
    if (!m_viewer || m_viewer->GetContext().IsNull()) return;
    ClearPreviewDisplay();
    for (auto const& [key, val] : m_displayedElements) {
        m_viewer->GetContext()->Remove(val, Standard_False);
    }
    m_displayedElements.clear();
    m_viewer->GetView()->Redraw();
}

} // namespace cad_ui

#include "SketchMode.moc"