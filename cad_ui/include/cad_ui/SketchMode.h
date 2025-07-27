#pragma once

#include <QObject>
#include <QWidget>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <gp_Ax3.hxx>
#include <gp_Lin.hxx>
#include <gp_Pnt2d.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <TopAbs.hxx>
#include <V3d_View.hxx>
#include <memory>
#include <vector>

#include "cad_core/Shape.h"
#include "cad_sketch/Sketch.h"
#include "cad_sketch/SketchLine.h"

#include <TopoDS_Edge.hxx>
#include <AIS_Shape.hxx>
#include <map> 

namespace cad_ui {

// 前向声明
class QtOccView;

/**
 * @class SketchRectangleTool
 * @brief 矩形绘制工具
 */
class SketchRectangleTool : public QObject {
    Q_OBJECT

public:
    explicit SketchRectangleTool(QObject* parent = nullptr);
    
    void StartDrawing(const QPoint& startPoint);
    void UpdateDrawing(const QPoint& currentPoint);
    void FinishDrawing(const QPoint& endPoint);
    void CancelDrawing();
    
    bool IsDrawing() const { return m_isDrawing; }
    
    // 将屏幕坐标转换为草图平面坐标
    void SetSketchPlane(const gp_Pln& plane);
    void SetView(Handle(V3d_View) view);
    
    // 获取当前绘制的矩形
    std::vector<cad_sketch::SketchLinePtr> GetCurrentRectangle() const;

signals:
    void rectangleCreated(const std::vector<cad_sketch::SketchLinePtr>& lines);
    void previewUpdated(const std::vector<cad_sketch::SketchLinePtr>& previewLines);
    void drawingCancelled();

private:
    bool m_isDrawing;
    QPoint m_startPoint;
    QPoint m_currentPoint;
    gp_Pln m_sketchPlane;
    Handle(V3d_View) m_view;
    
    std::vector<cad_sketch::SketchLinePtr> m_currentLines;
    
    // 辅助方法
    gp_Pnt ScreenToSketchPlane(const QPoint& screenPoint);
    std::vector<cad_sketch::SketchLinePtr> CreateRectangleLines(const gp_Pnt& point1, const gp_Pnt& point2);
};


class SketchLineTool : public QObject {
    Q_OBJECT

public:
    explicit SketchLineTool(QObject* parent = nullptr);

    void StartDrawing(const QPoint& startPoint);
    void UpdateDrawing(const QPoint& currentPoint);
    void FinishDrawing(const QPoint& endPoint);
    void CancelDrawing();

    bool IsDrawing() const { return m_isDrawing; }

    void SetSketchPlane(const gp_Pln& plane);
    void SetView(Handle(V3d_View) view);

signals:
    void lineCreated(const cad_sketch::SketchLinePtr& line);
    void previewUpdated(const std::vector<cad_sketch::SketchLinePtr>& previewLines);
    void drawingCancelled();

private:
    bool m_isDrawing;
    gp_Pnt m_startPoint3d; // 我们直接存储3D点

    gp_Pln m_sketchPlane;
    Handle(V3d_View) m_view;

    gp_Pnt ScreenToSketchPlane(const QPoint& screenPoint);
};



/**
 * @class SketchMode
 * @brief 草图模式管理器
 */
class SketchMode : public QObject {
    Q_OBJECT

public:
    explicit SketchMode(QtOccView* viewer, QObject* parent = nullptr);
    ~SketchMode() = default;
    
    // 草图模式控制
    bool EnterSketchMode(const TopoDS_Face& face);
    void ExitSketchMode();
    bool IsInSketchMode() const { return m_isActive; }
    
    // 获取当前草图信息
    const cad_sketch::SketchPtr& GetCurrentSketch() const { return m_currentSketch; }
    const gp_Pln& GetSketchPlane() const { return m_sketchPlane; }
    const TopoDS_Face& GetSketchFace() const { return m_sketchFace; }
    
    // 绘制工具
    void StartRectangleTool();
	void StartLineTool();   
    void StopCurrentTool();
    
    // 鼠标事件处理
    void HandleMousePress(QMouseEvent* event);
    void HandleMouseMove(QMouseEvent* event);
    void HandleMouseRelease(QMouseEvent* event);
    void HandleKeyPress(QKeyEvent* event);

signals:
    void sketchModeEntered();
    void sketchModeExited();
    void sketchElementCreated(cad_sketch::SketchElementPtr element);
    void statusMessageChanged(const QString& message);

private slots:
    void OnRectangleCreated(const std::vector<cad_sketch::SketchLinePtr>& lines);
    void OnDrawingCancelled();

private:
    QtOccView* m_viewer;
    bool m_isActive;
    
    // 草图相关
    cad_sketch::SketchPtr m_currentSketch;
    TopoDS_Face m_sketchFace;
    gp_Pln m_sketchPlane;
    gp_Ax3 m_sketchCS; // 草图坐标系
    
    // 视图保存/恢复
    gp_Pnt m_savedEye;
    gp_Pnt m_savedAt;
    gp_Dir m_savedUp;
    double m_savedScale;
    
    // 绘制工具
    enum class ActiveTool { None, Rectangle, Line }; 
    ActiveTool m_activeTool;                       
    std::unique_ptr<SketchRectangleTool> m_rectangleTool;
    std::unique_ptr<SketchLineTool> m_lineTool;    
    
    // 私有方法
    void SetupSketchPlane(const TopoDS_Face& face);
    void SetupSketchView();
    void RestoreView();
    void CreateSketchCoordinateSystem();
    gp_Pln ExtractPlaneFromFace(const TopoDS_Face& face);

    // 用于存储草图元素和其对应的可显示对象之间的映射关系
    std::map<cad_sketch::SketchElementPtr, Handle(AIS_Shape)> m_displayedElements;

    // 用于存储正在绘制的预览图形，方便快速清除
    std::vector<Handle(AIS_Shape)> m_previewElements;
    TopoDS_Edge ConvertLineToEdge(const cad_sketch::SketchLinePtr& line) const;

    // 显示一个草图元素
    void DisplaySketchElement(const cad_sketch::SketchElementPtr& element);

    // 更新预览图形
    void UpdatePreview(const std::vector<cad_sketch::SketchLinePtr>& previewLines);

    // 清除所有草图的显示
    void ClearAllSketchDisplay();

    // 清除预览图形的显示
    void ClearPreviewDisplay();

};

} // namespace cad_ui