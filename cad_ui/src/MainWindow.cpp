﻿#include "cad_ui/MainWindow.h"
#include "cad_ui/ExportDialog.h"
#include "cad_ui/AboutDialog.h"
#include "cad_ui/CreatePrimitiveDialog.h"
#include "cad_core/CreateBoxCommand.h"
#include "cad_core/CreateCylinderCommand.h"
#include "cad_core/CreateSphereCommand.h"
#include "cad_core/CreateTorusCommand.h"
#include "cad_core/OCAFManager.h"
#include "cad_core/ShapeFactory.h"
#include "cad_core/BooleanOperations.h"
#include "cad_core/FilletChamferOperations.h"
#include "cad_core/SelectionManager.h"
#include "cad_feature/ExtrudeFeature.h"
#include <TopoDS.hxx>

#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QCloseEvent>
#include <QSplitter>
#include <QSettings>
#include <QTabWidget>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QLabel>
#include <map>
#pragma execution_character_set("utf-8")

namespace cad_ui {

MainWindow::MainWindow(QWidget* parent) 
    : QMainWindow(parent), m_tabWidget(nullptr), m_documentModified(false), 
      m_isDragging(false), m_dragStartPosition(), m_titleBar(nullptr),
      m_titleLabel(nullptr), m_minimizeButton(nullptr), m_maximizeButton(nullptr),
      m_closeButton(nullptr), m_currentBooleanDialog(nullptr), m_currentFilletChamferDialog(nullptr),
      m_currentTransformDialog(nullptr), m_previewActive(false), 
      m_waitingForFaceSelection(false) {
    
    // Load modern flat stylesheet
    QFile styleFile(":/resources/styles.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream stream(&styleFile);
        QString style = stream.readAll();
        this->setStyleSheet(style);
        qDebug() << "Stylesheet loaded successfully, length:" << style.length();
    } else {
        qDebug() << "Failed to load stylesheet from resources";
        // Fallback: try to load from file system for development
        QFile fallbackFile("C:\\Users\\Administrator\\source\\repos\\draft\\draft\\cad_ui\\resources\\styles.qss");
        if (fallbackFile.open(QFile::ReadOnly | QFile::Text)) {
            QTextStream fallbackStream(&fallbackFile);
            QString fallbackStyle = fallbackStream.readAll();
            this->setStyleSheet(fallbackStyle);
            qDebug() << "Fallback stylesheet loaded, length:" << fallbackStyle.length();
        }
    }
    
    // Initialize managers
    m_commandManager = std::make_unique<cad_core::CommandManager>();
    m_ocafManager = std::make_unique<cad_core::OCAFManager>();
    m_featureManager = std::make_unique<cad_feature::FeatureManager>();
    
    // Create UI components
    CreateActions();
    CreateMenus();
    
    // Create selection mode combo box before toolbars
    CreateSelectionModeCombo();
    
    CreateToolBars();
    CreateStatusBar();
    CreateDockWidgets();
    CreateTitleBar();
    CreateConsole();
    
    // Create multi-document tab interface
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setObjectName("documentTabs");

    
    // Create first document tab
    m_viewer = new QtOccView(this);
    m_viewer->setObjectName("viewer3D");
    m_tabWidget->addTab(m_viewer, "Document 1");
    
    // Create main splitter with viewer and console
    m_mainSplitter = new QSplitter(Qt::Vertical, this);
    m_mainSplitter->addWidget(m_tabWidget);
    m_mainSplitter->addWidget(m_console);
    m_mainSplitter->setStretchFactor(0, 3); // Give viewer more space
    m_mainSplitter->setStretchFactor(1, 1); // Console gets less space
    
    setCentralWidget(m_mainSplitter);
    
    // Initialize theme manager
    m_themeManager = new ThemeManager(this);
    
    // Initialize dialog pointers to null
    m_currentBooleanDialog = nullptr;
    m_currentFilletChamferDialog = nullptr;
    
    // Connect signals
    ConnectSignals();
    
    // Connect tab widget signals
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::CloseDocumentTab);
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MainWindow::OnTabChanged);
    
    // Set window properties - frameless window
    setWindowTitle("Ander CAD");
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setMinimumSize(800, 600);
    resize(1200, 800);
    
    // Enable mouse tracking for window moving
    setMouseTracking(true);
    
    // Update UI
    UpdateActions();
    UpdateWindowTitle();
}

bool MainWindow::Initialize() {
    
    // Initialize OCAF manager
    if (!m_ocafManager->Initialize()) {
        QMessageBox::critical(this, "Error", "Failed to initialize OCAF document manager");
        return false;
    }
    
    // Create initial document for undo/redo functionality
    if (!m_ocafManager->NewDocument()) {
        QMessageBox::critical(this, "Error", "Failed to create new OCAF document");
        return false;
    }
    
    // Set initial view and render
    m_viewer->FitAll();
    m_viewer->RedrawAll();  // 确保坐标轴立即显示
    
    return true;
}

void MainWindow::CreateActions() {
    // File actions
    m_newAction = new QAction("&New", this);
    m_newAction->setShortcut(QKeySequence::New);
    m_newAction->setStatusTip("Create a new document");
    
    m_openAction = new QAction("&Open...", this);
    m_openAction->setShortcut(QKeySequence::Open);
    m_openAction->setStatusTip("Open an existing document");
    
    m_saveAction = new QAction("&Save", this);
    m_saveAction->setShortcut(QKeySequence::Save);
    m_saveAction->setStatusTip("Save the document");
    
    m_saveAsAction = new QAction("Save &As...", this);
    m_saveAsAction->setShortcut(QKeySequence::SaveAs);
    m_saveAsAction->setStatusTip("Save the document with a new name");
    
    m_exitAction = new QAction("E&xit", this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    m_exitAction->setStatusTip("Exit the application");
    
    // Edit actions
    m_undoAction = new QAction("&Undo", this);
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setStatusTip("Undo the last operation");
    
    m_redoAction = new QAction("&Redo", this);
    m_redoAction->setShortcut(QKeySequence("Ctrl+Y"));
    m_redoAction->setStatusTip("Redo the last undone operation");
    
    // View actions
    m_fitAllAction = new QAction("Fit &All", this);
    m_fitAllAction->setShortcut(QKeySequence("F"));
    m_fitAllAction->setStatusTip("Fit all objects in view");
    
    m_zoomInAction = new QAction("Zoom &In", this);
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    m_zoomInAction->setStatusTip("Zoom in");
    
    m_zoomOutAction = new QAction("Zoom &Out", this);
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    m_zoomOutAction->setStatusTip("Zoom out");
    
    // View mode actions
    m_viewWireframeAction = new QAction("&Wireframe", this);
    m_viewWireframeAction->setShortcut(QKeySequence("W"));
    m_viewWireframeAction->setCheckable(true);
    m_viewWireframeAction->setStatusTip("Show wireframe view");
    
    m_viewShadedAction = new QAction("&Shaded", this);
    m_viewShadedAction->setShortcut(QKeySequence("S"));
    m_viewShadedAction->setCheckable(true);
    m_viewShadedAction->setChecked(true);
    m_viewShadedAction->setStatusTip("Show shaded view");
    
    m_viewModeGroup = new QActionGroup(this);
    m_viewModeGroup->addAction(m_viewWireframeAction);
    m_viewModeGroup->addAction(m_viewShadedAction);
    
    // Projection mode actions
    m_viewOrthographicAction = new QAction("&Orthographic", this);
    m_viewOrthographicAction->setCheckable(true);
    m_viewOrthographicAction->setChecked(true);
    m_viewOrthographicAction->setStatusTip("Orthographic projection");
    
    m_viewPerspectiveAction = new QAction("&Perspective", this);
    m_viewPerspectiveAction->setCheckable(true);
    m_viewPerspectiveAction->setStatusTip("Perspective projection");
    
    m_projectionModeGroup = new QActionGroup(this);
    m_projectionModeGroup->addAction(m_viewOrthographicAction);
    m_projectionModeGroup->addAction(m_viewPerspectiveAction);
    
    // Create actions
    m_createBoxAction = new QAction("Create &Box", this);
    m_createBoxAction->setStatusTip("Create a box");
    
    m_createCylinderAction = new QAction("Create &Cylinder", this);
    m_createCylinderAction->setStatusTip("Create a cylinder");
    
    m_createSphereAction = new QAction("Create &Sphere", this);
    m_createSphereAction->setStatusTip("Create a sphere");

    m_createTorusAction = new QAction("Create &Torus", this); 
    m_createTorusAction->setStatusTip("Create a torus");     
    
    m_createExtrudeAction = new QAction("Create &Extrude", this);
    m_createExtrudeAction->setStatusTip("Create an extrude feature");
    
    // Boolean operations
    m_booleanUnionAction = new QAction("&Union", this);
    m_booleanUnionAction->setStatusTip("Unite selected shapes");
    
    m_booleanIntersectionAction = new QAction("&Intersection", this);
    m_booleanIntersectionAction->setStatusTip("Intersect selected shapes");
    
    m_booleanDifferenceAction = new QAction("&Difference", this);
    m_booleanDifferenceAction->setStatusTip("Subtract one shape from another");
    
    // Fillet and chamfer operations
    m_filletAction = new QAction("&Fillet", this);
    m_filletAction->setStatusTip("Add fillet to selected edges");
    
    m_chamferAction = new QAction("&Chamfer", this);
    m_chamferAction->setStatusTip("Add chamfer to selected edges");
    
    // Transform actions
    m_transformAction = new QAction("&Transform...", this);
    m_transformAction->setShortcut(QKeySequence("Ctrl+T"));
    m_transformAction->setStatusTip("Transform objects (translate, rotate, scale)");
    
    // Sketch actions
    m_enterSketchAction = new QAction("Enter &Sketch", this);
    m_enterSketchAction->setShortcut(QKeySequence("Ctrl+Shift+S"));
    m_enterSketchAction->setStatusTip("Enter sketch mode");
    
    m_exitSketchAction = new QAction("E&xit Sketch", this);
    m_exitSketchAction->setShortcut(QKeySequence("Escape"));
    m_exitSketchAction->setStatusTip("Exit sketch mode");
    m_exitSketchAction->setEnabled(false);  // 初始禁用
    
    m_sketchRectangleAction = new QAction("&Rectangle", this);
    m_sketchRectangleAction->setShortcut(QKeySequence("R"));
    m_sketchRectangleAction->setStatusTip("Draw rectangle in sketch mode");
    m_sketchRectangleAction->setEnabled(false);  // 初始禁用
    
    m_sketchLineAction = new QAction("&Line", this);
    m_sketchLineAction->setShortcut(QKeySequence("L"));
    m_sketchLineAction->setStatusTip("Draw line in sketch mode");
    m_sketchLineAction->setEnabled(false);  // 初始禁用

    m_sketchCircleAction = new QAction("&Circle", this);
    m_sketchCircleAction->setShortcut(QKeySequence("C"));
    m_sketchCircleAction->setStatusTip("Draw circle in sketch mode");
    m_sketchCircleAction->setEnabled(false);  // 初始禁用

    // Selection mode now handled by combo box - old actions commented out for testing
    
    // Selection mode group now handled by combo box
    
    // Theme actions
    m_darkThemeAction = new QAction("&Dark Theme", this);
    m_darkThemeAction->setCheckable(true);
    m_darkThemeAction->setStatusTip("Use dark theme");
    
    m_lightThemeAction = new QAction("&Light Theme", this);
    m_lightThemeAction->setCheckable(true);
    m_lightThemeAction->setChecked(true);
    m_lightThemeAction->setStatusTip("Use light theme");
    
    m_themeGroup = new QActionGroup(this);
    m_themeGroup->addAction(m_darkThemeAction);
    m_themeGroup->addAction(m_lightThemeAction);
    
    // Help actions
    m_aboutAction = new QAction("&About", this);
    m_aboutAction->setStatusTip("Show the application's About box");
    
    m_aboutQtAction = new QAction("About &Qt", this);
    m_aboutQtAction->setStatusTip("Show the Qt library's About box");
}

void MainWindow::CreateMenus() {
    // File menu
    QMenu* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction(m_newAction);
    fileMenu->addAction(m_openAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_saveAction);
    fileMenu->addAction(m_saveAsAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_exitAction);
    
    // Edit menu
    QMenu* editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction(m_undoAction);
    editMenu->addAction(m_redoAction);
    
    // View menu
    QMenu* viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction(m_fitAllAction);
    viewMenu->addAction(m_zoomInAction);
    viewMenu->addAction(m_zoomOutAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_viewWireframeAction);
    viewMenu->addAction(m_viewShadedAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_viewOrthographicAction);
    viewMenu->addAction(m_viewPerspectiveAction);
    
    // Create menu
    QMenu* createMenu = menuBar()->addMenu("&Create");
    createMenu->addAction(m_createBoxAction);
    createMenu->addAction(m_createCylinderAction);
    createMenu->addAction(m_createSphereAction);
    createMenu->addAction(m_createTorusAction);
    createMenu->addSeparator();
    createMenu->addAction(m_createExtrudeAction);
    
    // Boolean menu
    QMenu* booleanMenu = menuBar()->addMenu("&Boolean");
    booleanMenu->addAction(m_booleanUnionAction);
    booleanMenu->addAction(m_booleanIntersectionAction);
    booleanMenu->addAction(m_booleanDifferenceAction);
    
    // Modify menu
    QMenu* modifyMenu = menuBar()->addMenu("&Modify");
    modifyMenu->addAction(m_filletAction);
    modifyMenu->addAction(m_chamferAction);
    modifyMenu->addSeparator();
    modifyMenu->addAction(m_transformAction);
    
    // Sketch menu
    QMenu* sketchMenu = menuBar()->addMenu("&Sketch");
    sketchMenu->addAction(m_enterSketchAction);
    sketchMenu->addAction(m_exitSketchAction);
    sketchMenu->addSeparator();
    sketchMenu->addAction(m_sketchRectangleAction);
	sketchMenu->addAction(m_sketchLineAction);
	sketchMenu->addAction(m_sketchCircleAction);
    
    // Selection menu - now handled by combo box in toolbar
    
    // Tools menu
    QMenu* toolsMenu = menuBar()->addMenu("&Tools");
    toolsMenu->addAction(m_darkThemeAction);
    toolsMenu->addAction(m_lightThemeAction);
    
    // Help menu
    QMenu* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction(m_aboutAction);
    helpMenu->addAction(m_aboutQtAction);
}

void MainWindow::CreateToolBars() {
    // Create main toolbar area widget with tabs
    QWidget* toolBarArea = new QWidget(this);
    toolBarArea->setObjectName("toolBarArea");
    toolBarArea->setMaximumHeight(150);
    toolBarArea->setMinimumHeight(150);
    
    // Create tab widget for organizing tools
    QTabWidget* toolTabWidget = new QTabWidget(toolBarArea);
    toolTabWidget->setObjectName("toolTabWidget");
    toolTabWidget->setTabPosition(QTabWidget::North);
    
    // File Tab - File operations and undo/redo
    QWidget* fileTab = new QWidget();
    QHBoxLayout* fileLayout = new QHBoxLayout(fileTab);
    fileLayout->setContentsMargins(5, 2, 5, 2);
    fileLayout->setSpacing(3);
    
    // File operations group
    QFrame* fileOpsFrame = new QFrame();
    fileOpsFrame->setFrameStyle(QFrame::StyledPanel);
    QVBoxLayout* fileOpsLayout = new QVBoxLayout(fileOpsFrame);
    fileOpsLayout->setContentsMargins(2, 1, 2, 2);
    fileOpsLayout->setSpacing(1);
    
    QLabel* fileLabel = new QLabel("文件");
    fileLabel->setAlignment(Qt::AlignCenter);
    fileOpsLayout->addWidget(fileLabel);
    QHBoxLayout* fileButtonsLayout = new QHBoxLayout();
    fileButtonsLayout->setSpacing(2);
    
    QToolButton* newBtn = new QToolButton();
    newBtn->setDefaultAction(m_newAction);
    newBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    newBtn->setMinimumSize(90, 90);
    newBtn->setMaximumSize(90, 90);
    fileButtonsLayout->addWidget(newBtn);
    
    QToolButton* openBtn = new QToolButton();
    openBtn->setDefaultAction(m_openAction);
    openBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    openBtn->setMinimumSize(90, 90);
    openBtn->setMaximumSize(90, 90);
    fileButtonsLayout->addWidget(openBtn);
    
    QToolButton* saveBtn = new QToolButton();
    saveBtn->setDefaultAction(m_saveAction);
    saveBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    saveBtn->setMinimumSize(90, 90);
    saveBtn->setMaximumSize(90, 90);
    fileButtonsLayout->addWidget(saveBtn);
    
    fileOpsLayout->addLayout(fileButtonsLayout);
    fileLayout->addWidget(fileOpsFrame);
    
    // History operations group
    QFrame* historyFrame = new QFrame();
    historyFrame->setFrameStyle(QFrame::StyledPanel);
    QVBoxLayout* historyLayout = new QVBoxLayout(historyFrame);
    historyLayout->setContentsMargins(2, 1, 2, 2);
    historyLayout->setSpacing(1);
    
    QLabel* historyLabel = new QLabel("历史");
    historyLabel->setAlignment(Qt::AlignCenter);
    historyLayout->addWidget(historyLabel);
    QHBoxLayout* historyButtonsLayout = new QHBoxLayout();
    historyButtonsLayout->setSpacing(2);
    
    QToolButton* undoBtn = new QToolButton();
    undoBtn->setDefaultAction(m_undoAction);
    undoBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    undoBtn->setMinimumSize(90, 90);
    undoBtn->setMaximumSize(90, 90);
    historyButtonsLayout->addWidget(undoBtn);
    
    QToolButton* redoBtn = new QToolButton();
    redoBtn->setDefaultAction(m_redoAction);
    redoBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    redoBtn->setMinimumSize(90, 90);
    redoBtn->setMaximumSize(90, 90);
    historyButtonsLayout->addWidget(redoBtn);
    
    historyLayout->addLayout(historyButtonsLayout);
    fileLayout->addWidget(historyFrame);
    
    fileLayout->addStretch();
    toolTabWidget->addTab(fileTab, "文件");
    
    // Design Tab - Primitive creation
    QWidget* designTab = new QWidget();
    QHBoxLayout* designLayout = new QHBoxLayout(designTab);
    designLayout->setContentsMargins(5, 2, 5, 2);
    designLayout->setSpacing(3);
    
    // Primitives group
    QFrame* primitivesFrame = new QFrame();
    primitivesFrame->setFrameStyle(QFrame::StyledPanel);
    QVBoxLayout* primitivesLayout = new QVBoxLayout(primitivesFrame);
    primitivesLayout->setContentsMargins(2, 1, 2, 2);
    primitivesLayout->setSpacing(1);
    
    QLabel* primitivesLabel = new QLabel("基本形状");
    primitivesLabel->setAlignment(Qt::AlignCenter);
    primitivesLayout->addWidget(primitivesLabel);
    QHBoxLayout* primitivesButtonsLayout = new QHBoxLayout();
    primitivesButtonsLayout->setSpacing(2);
    
    QToolButton* boxBtn = new QToolButton();
    boxBtn->setDefaultAction(m_createBoxAction);
    boxBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    primitivesButtonsLayout->addWidget(boxBtn);
    
    QToolButton* cylinderBtn = new QToolButton();
    cylinderBtn->setDefaultAction(m_createCylinderAction);
    cylinderBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    primitivesButtonsLayout->addWidget(cylinderBtn);
    
    QToolButton* sphereBtn = new QToolButton();
    sphereBtn->setDefaultAction(m_createSphereAction);
    sphereBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    primitivesButtonsLayout->addWidget(sphereBtn);
    
    QToolButton* torusBtn = new QToolButton();
    torusBtn->setDefaultAction(m_createTorusAction);
    torusBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    primitivesButtonsLayout->addWidget(torusBtn);

    primitivesLayout->addLayout(primitivesButtonsLayout);
    designLayout->addWidget(primitivesFrame);
    
    // Features group
    QFrame* featuresFrame = new QFrame();
    featuresFrame->setFrameStyle(QFrame::StyledPanel);
    QVBoxLayout* featuresLayout = new QVBoxLayout(featuresFrame);
    featuresLayout->setContentsMargins(2, 1, 2, 2);
    featuresLayout->setSpacing(1);
    
    QLabel* featuresLabel = new QLabel("特征");
    featuresLabel->setAlignment(Qt::AlignCenter);
    featuresLayout->addWidget(featuresLabel);
    QHBoxLayout* featuresButtonsLayout = new QHBoxLayout();
    featuresButtonsLayout->setSpacing(2);
    
    QToolButton* extrudeBtn = new QToolButton();
    extrudeBtn->setDefaultAction(m_createExtrudeAction);
    extrudeBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    featuresButtonsLayout->addWidget(extrudeBtn);
    
    featuresLayout->addLayout(featuresButtonsLayout);
    designLayout->addWidget(featuresFrame);
    
    designLayout->addStretch();
    toolTabWidget->addTab(designTab, "设计");
    
    // Modify Tab - Boolean operations and modifications
    QWidget* modifyTab = new QWidget();
    QHBoxLayout* modifyLayout = new QHBoxLayout(modifyTab);
    modifyLayout->setContentsMargins(5, 2, 5, 2);
    modifyLayout->setSpacing(3);
    
    // Boolean operations group
    QFrame* booleanFrame = new QFrame();
    booleanFrame->setFrameStyle(QFrame::StyledPanel);
    QVBoxLayout* booleanLayout = new QVBoxLayout(booleanFrame);
    booleanLayout->setContentsMargins(2, 1, 2, 2);
    booleanLayout->setSpacing(1);
    
    QLabel* booleanLabel = new QLabel("布尔运算");
    booleanLabel->setAlignment(Qt::AlignCenter);
    booleanLayout->addWidget(booleanLabel);
    QHBoxLayout* booleanButtonsLayout = new QHBoxLayout();
    booleanButtonsLayout->setSpacing(2);
    
    QToolButton* unionBtn = new QToolButton();
    unionBtn->setDefaultAction(m_booleanUnionAction);
    unionBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    booleanButtonsLayout->addWidget(unionBtn);
    
    QToolButton* intersectionBtn = new QToolButton();
    intersectionBtn->setDefaultAction(m_booleanIntersectionAction);
    intersectionBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    booleanButtonsLayout->addWidget(intersectionBtn);
    
    QToolButton* differenceBtn = new QToolButton();
    differenceBtn->setDefaultAction(m_booleanDifferenceAction);
    differenceBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    booleanButtonsLayout->addWidget(differenceBtn);
    
    booleanLayout->addLayout(booleanButtonsLayout);
    modifyLayout->addWidget(booleanFrame);
    
    // Modifications group
    QFrame* modificationsFrame = new QFrame();
    modificationsFrame->setFrameStyle(QFrame::StyledPanel);
    QVBoxLayout* modificationsLayout = new QVBoxLayout(modificationsFrame);
    modificationsLayout->setContentsMargins(2, 1, 2, 2);
    modificationsLayout->setSpacing(1);
    
    QLabel* modificationsLabel = new QLabel("修改");
    modificationsLabel->setAlignment(Qt::AlignCenter);
    modificationsLayout->addWidget(modificationsLabel);
    QHBoxLayout* modificationsButtonsLayout = new QHBoxLayout();
    modificationsButtonsLayout->setSpacing(2);
    
    QToolButton* filletBtn = new QToolButton();
    filletBtn->setDefaultAction(m_filletAction);
    filletBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    modificationsButtonsLayout->addWidget(filletBtn);
    
    QToolButton* chamferBtn = new QToolButton();
    chamferBtn->setDefaultAction(m_chamferAction);
    chamferBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    modificationsButtonsLayout->addWidget(chamferBtn);
    
    QToolButton* transformBtn = new QToolButton();
    transformBtn->setDefaultAction(m_transformAction);
    transformBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    modificationsButtonsLayout->addWidget(transformBtn);
    
    modificationsLayout->addLayout(modificationsButtonsLayout);
    modifyLayout->addWidget(modificationsFrame);
    
    modifyLayout->addStretch();
    toolTabWidget->addTab(modifyTab, "修改");
    
    // View Tab - View controls and selection
    QWidget* viewTab = new QWidget();
    QHBoxLayout* viewLayout = new QHBoxLayout(viewTab);
    viewLayout->setContentsMargins(5, 2, 5, 2);
    viewLayout->setSpacing(3);
    
    // Selection group
    QFrame* selectionFrame = new QFrame();
    selectionFrame->setFrameStyle(QFrame::StyledPanel);
    QVBoxLayout* selectionLayout = new QVBoxLayout(selectionFrame);
    selectionLayout->setContentsMargins(2, 1, 2, 2);
    selectionLayout->setSpacing(1);
    
    QLabel* selectionLabel = new QLabel("选择");
    selectionLabel->setAlignment(Qt::AlignCenter);
    selectionLayout->addWidget(selectionLabel);
    
    // Add selection mode combo box
    if (m_selectionModeCombo) {
        m_selectionModeCombo->setMinimumWidth(100);
        selectionLayout->addWidget(m_selectionModeCombo);
    }
    
    viewLayout->addWidget(selectionFrame);
    
    // View controls group
    QFrame* viewControlsFrame = new QFrame();
    viewControlsFrame->setFrameStyle(QFrame::StyledPanel);
    QVBoxLayout* viewControlsLayout = new QVBoxLayout(viewControlsFrame);
    viewControlsLayout->setContentsMargins(2, 1, 2, 2);
    viewControlsLayout->setSpacing(1);
    
    QLabel* viewControlsLabel = new QLabel("视图");
    viewControlsLabel->setAlignment(Qt::AlignCenter);
    viewControlsLayout->addWidget(viewControlsLabel);
    QHBoxLayout* viewControlsButtonsLayout = new QHBoxLayout();
    viewControlsButtonsLayout->setSpacing(2);
    
    QToolButton* fitAllBtn = new QToolButton();
    fitAllBtn->setDefaultAction(m_fitAllAction);
    fitAllBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    viewControlsButtonsLayout->addWidget(fitAllBtn);
    
    QToolButton* wireframeBtn = new QToolButton();
    wireframeBtn->setDefaultAction(m_viewWireframeAction);
    wireframeBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    viewControlsButtonsLayout->addWidget(wireframeBtn);
    
    QToolButton* shadedBtn = new QToolButton();
    shadedBtn->setDefaultAction(m_viewShadedAction);
    shadedBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    viewControlsButtonsLayout->addWidget(shadedBtn);
    
    viewControlsLayout->addLayout(viewControlsButtonsLayout);
    viewLayout->addWidget(viewControlsFrame);
    
    viewLayout->addStretch();
    toolTabWidget->addTab(viewTab, "视图");
    
    // Sketch Tab - Sketch mode controls
    QWidget* sketchTab = new QWidget();
    QHBoxLayout* sketchLayout = new QHBoxLayout(sketchTab);
    sketchLayout->setContentsMargins(5, 2, 5, 2);
    sketchLayout->setSpacing(3);
    
    // Sketch mode group
    QFrame* sketchModeFrame = new QFrame();
    sketchModeFrame->setFrameStyle(QFrame::StyledPanel);
    QVBoxLayout* sketchModeLayout = new QVBoxLayout(sketchModeFrame);
    sketchModeLayout->setContentsMargins(2, 1, 2, 2);
    sketchModeLayout->setSpacing(1);
    
    QLabel* sketchModeLabel = new QLabel("草图模式");
    sketchModeLabel->setAlignment(Qt::AlignCenter);
    sketchModeLayout->addWidget(sketchModeLabel);
    QHBoxLayout* sketchModeButtonsLayout = new QHBoxLayout();
    sketchModeButtonsLayout->setSpacing(2);
    
    QToolButton* enterSketchBtn = new QToolButton();
    enterSketchBtn->setDefaultAction(m_enterSketchAction);
    enterSketchBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    sketchModeButtonsLayout->addWidget(enterSketchBtn);
    
    QToolButton* exitSketchBtn = new QToolButton();
    exitSketchBtn->setDefaultAction(m_exitSketchAction);
    exitSketchBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    sketchModeButtonsLayout->addWidget(exitSketchBtn);
    
    sketchModeLayout->addLayout(sketchModeButtonsLayout);
    sketchLayout->addWidget(sketchModeFrame);
    
    // Sketch tools group
    QFrame* sketchToolsFrame = new QFrame();
    sketchToolsFrame->setFrameStyle(QFrame::StyledPanel);
    QVBoxLayout* sketchToolsLayout = new QVBoxLayout(sketchToolsFrame);
    sketchToolsLayout->setContentsMargins(2, 1, 2, 2);
    sketchToolsLayout->setSpacing(1);
    
    QLabel* sketchToolsLabel = new QLabel("绘制工具");
    sketchToolsLabel->setAlignment(Qt::AlignCenter);
    sketchToolsLayout->addWidget(sketchToolsLabel);
    QHBoxLayout* sketchToolsButtonsLayout = new QHBoxLayout();
    sketchToolsButtonsLayout->setSpacing(2);
    
    QToolButton* rectangleBtn = new QToolButton();
    rectangleBtn->setDefaultAction(m_sketchRectangleAction);
    rectangleBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    sketchToolsButtonsLayout->addWidget(rectangleBtn);
    
    QToolButton* lineBtn = new QToolButton();
    lineBtn->setDefaultAction(m_sketchLineAction);
    lineBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    sketchToolsButtonsLayout->addWidget(lineBtn);

    QToolButton* circleBtn = new QToolButton();
    circleBtn->setDefaultAction(m_sketchCircleAction);
    circleBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    sketchToolsButtonsLayout->addWidget(circleBtn);

    sketchToolsLayout->addLayout(sketchToolsButtonsLayout);
    sketchLayout->addWidget(sketchToolsFrame);
    
    sketchLayout->addStretch();
    toolTabWidget->addTab(sketchTab, "草图");
    
    // Set layout for toolbar area
    QVBoxLayout* toolBarAreaLayout = new QVBoxLayout(toolBarArea);
    toolBarAreaLayout->setContentsMargins(0, 0, 0, 0);
    toolBarAreaLayout->addWidget(toolTabWidget);
    
    // Add toolbar area as a toolbar to maintain proper positioning
    QToolBar* containerToolBar = addToolBar("Container");
    containerToolBar->addWidget(toolBarArea);
    containerToolBar->setMovable(false);
    containerToolBar->setObjectName("containerToolBar");
    
    // Set selection mode buttons object names for styling
    // Old selection mode button styling removed - now using combo box
    
    // Set boolean operation buttons object names
    m_booleanUnionAction->setObjectName("booleanButton");
    m_booleanIntersectionAction->setObjectName("booleanButton");
    m_booleanDifferenceAction->setObjectName("booleanButton");
    
    // Set modify operation buttons object names
    m_filletAction->setObjectName("modifyButton");
    m_chamferAction->setObjectName("modifyButton");
}

void MainWindow::CreateStatusBar() {
    statusBar()->showMessage("Ready");
}

void MainWindow::CreateDockWidgets() {
    // Document tree dock
    m_documentDock = new QDockWidget("Document Tree", this);
    m_documentTree = new DocumentTree(this);
    m_documentDock->setWidget(m_documentTree);
    addDockWidget(Qt::LeftDockWidgetArea, m_documentDock);
    
    // Property panel dock
    m_propertyDock = new QDockWidget("Properties", this);
    m_propertyPanel = new PropertyPanel(this);
    m_propertyDock->setWidget(m_propertyPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_propertyDock);
}

void MainWindow::ConnectSignals() {
    // File actions
    connect(m_newAction, &QAction::triggered, this, &MainWindow::OnNewDocument);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::OnOpenDocument);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::OnSaveDocument);
    connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::OnSaveDocumentAs);
    connect(m_exitAction, &QAction::triggered, this, &MainWindow::OnExit);
    
    // Edit actions
    connect(m_undoAction, &QAction::triggered, this, &MainWindow::OnUndo);
    connect(m_redoAction, &QAction::triggered, this, &MainWindow::OnRedo);
    
    // View actions
    connect(m_fitAllAction, &QAction::triggered, this, &MainWindow::OnFitAll);
    connect(m_zoomInAction, &QAction::triggered, this, &MainWindow::OnZoomIn);
    connect(m_zoomOutAction, &QAction::triggered, this, &MainWindow::OnZoomOut);
    connect(m_viewWireframeAction, &QAction::triggered, this, &MainWindow::OnViewWireframe);
    connect(m_viewShadedAction, &QAction::triggered, this, &MainWindow::OnViewShaded);
    connect(m_viewOrthographicAction, &QAction::triggered, this, &MainWindow::OnViewOrthographic);
    connect(m_viewPerspectiveAction, &QAction::triggered, this, &MainWindow::OnViewPerspective);
    
    // Create actions
    connect(m_createBoxAction, &QAction::triggered, this, &MainWindow::OnCreateBox);
    connect(m_createCylinderAction, &QAction::triggered, this, &MainWindow::OnCreateCylinder);
    connect(m_createSphereAction, &QAction::triggered, this, &MainWindow::OnCreateSphere);
    connect(m_createTorusAction, &QAction::triggered, this, &MainWindow::OnCreateTorus);
    connect(m_createExtrudeAction, &QAction::triggered, this, &MainWindow::OnCreateExtrude);
    
    // Boolean actions
    connect(m_booleanUnionAction, &QAction::triggered, this, &MainWindow::OnBooleanUnion);
    connect(m_booleanIntersectionAction, &QAction::triggered, this, &MainWindow::OnBooleanIntersection);
    connect(m_booleanDifferenceAction, &QAction::triggered, this, &MainWindow::OnBooleanDifference);
    
    // Modify actions
    connect(m_filletAction, &QAction::triggered, this, &MainWindow::OnFillet);
    connect(m_chamferAction, &QAction::triggered, this, &MainWindow::OnChamfer);
    
    // Transform operations
    connect(m_transformAction, &QAction::triggered, this, &MainWindow::OnTransformObjects);
    
    // Sketch actions
    connect(m_enterSketchAction, &QAction::triggered, this, &MainWindow::OnEnterSketchMode);
    connect(m_exitSketchAction, &QAction::triggered, this, &MainWindow::OnExitSketchMode);
    connect(m_sketchRectangleAction, &QAction::triggered, this, &MainWindow::OnSketchRectangleTool);
    connect(m_sketchLineAction, &QAction::triggered, this, &MainWindow::OnSketchLineTool);
    connect(m_sketchCircleAction, &QAction::triggered, this, &MainWindow::OnSketchCircleTool); 

    // Selection mode combo box connected in CreateSelectionModeCombo()
    
    // Theme actions
    connect(m_darkThemeAction, &QAction::triggered, this, &MainWindow::OnDarkTheme);
    connect(m_lightThemeAction, &QAction::triggered, this, &MainWindow::OnLightTheme);
    
    // Help actions
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::OnAbout);
    connect(m_aboutQtAction, &QAction::triggered, this, &MainWindow::OnAboutQt);
    
    // Viewer signals
    connect(m_viewer, &QtOccView::ShapeSelected, this, &MainWindow::OnShapeSelected);
    connect(m_viewer, &QtOccView::ViewChanged, this, &MainWindow::OnViewChanged);
    connect(m_viewer, &QtOccView::FaceSelected, this, &MainWindow::OnFaceSelected);
    connect(m_viewer, &QtOccView::SketchModeEntered, this, &MainWindow::OnSketchModeEntered);
    connect(m_viewer, &QtOccView::SketchModeExited, this, &MainWindow::OnSketchModeExited);
    
    // Mouse position signals
    //connect(m_viewer, &QtOccView::MousePositionChanged, m_statusBar, &StatusBar::updateMousePosition2D);
    //connect(m_viewer, &QtOccView::Mouse3DPositionChanged, m_statusBar, &StatusBar::updateMousePosition);
    
    // Document tree signals for selection synchronization
    connect(m_documentTree, &DocumentTree::ShapeSelected, this, &MainWindow::OnDocumentTreeShapeSelected);
    connect(m_documentTree, &DocumentTree::FeatureSelected, this, &MainWindow::OnDocumentTreeFeatureSelected);
}

void MainWindow::UpdateActions() {
    bool hasDocument = !m_currentFileName.isEmpty();
    bool canUndo = m_ocafManager->CanUndo();
    bool canRedo = m_ocafManager->CanRedo();
    
    m_saveAction->setEnabled(hasDocument && m_documentModified);
    m_saveAsAction->setEnabled(hasDocument);
    m_undoAction->setEnabled(canUndo);
    m_redoAction->setEnabled(canRedo);
    
    // Update action text based on availability
    m_undoAction->setText(canUndo ? "&Undo" : "&Undo");
    m_redoAction->setText(canRedo ? "&Redo" : "&Redo");
}

void MainWindow::RefreshUIFromOCAF() {
    if (!m_ocafManager) {
        return;
    }
    
    qDebug() << "Refreshing UI from OCAF document state";
    
    // Clear current UI state
    m_viewer->ClearShapes();
    m_documentTree->Clear();
    
    // Reload all shapes from OCAF document
    auto allShapes = m_ocafManager->GetAllShapes();
    qDebug() << "Found" << allShapes.size() << "shapes in OCAF document";
    
    for (const auto& shape : allShapes) {
        if (shape) {
            // Display in 3D viewer
            m_viewer->DisplayShape(shape);
            // Add to document tree
            m_documentTree->AddShape(shape);
        }
    }
    
    // Clear any selections
    m_viewer->ClearSelection();
    m_viewer->ClearEdgeSelection();
    
    // Force redraw
    m_viewer->RedrawAll();
    
    qDebug() << "UI refresh completed";
}

void MainWindow::UpdateWindowTitle() {
    QString title = "Ander CAD";
    if (!m_currentFileName.isEmpty()) {
        title += " - " + QFileInfo(m_currentFileName).baseName();
        if (m_documentModified) {
            title += " *";
        }
    }
    setWindowTitle(title);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (SaveChanges()) {
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    
    // Adjust title bar size
    if (m_titleBar) {
        m_titleBar->resize(width(), 40);
    }
    
    // Adjust menu bar position to be below title bar
    if (menuBar()) {
        menuBar()->move(0, 40);
        menuBar()->resize(width(), menuBar()->height());
    }
}

bool MainWindow::SaveChanges() {
    if (m_documentModified) {
        QMessageBox::StandardButton result = QMessageBox::question(this, 
            "Save Changes", 
            "The document has been modified. Do you want to save your changes?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        
        if (result == QMessageBox::Save) {
            return OnSaveDocument();
        } else if (result == QMessageBox::Cancel) {
            return false;
        }
    }
    return true;
}

void MainWindow::SetDocumentModified(bool modified) {
    m_documentModified = modified;
    UpdateActions();
    UpdateWindowTitle();
}

// Slot implementations
void MainWindow::OnNewDocument() {
    NewDocumentTab();
}

void MainWindow::OnOpenDocument() {
    // Placeholder implementation
    QMessageBox::information(this, "Open Document", "Open document functionality not implemented yet");
}

bool MainWindow::OnSaveDocument() {
    if (m_currentFileName.isEmpty()) {
        return OnSaveDocumentAs();
    }
    // Placeholder implementation
    SetDocumentModified(false);
    return true;
}

bool MainWindow::OnSaveDocumentAs() {
    // Placeholder implementation
    QString fileName = QFileDialog::getSaveFileName(this, "Save Document", "", "CAD Files (*.cad)");
    if (!fileName.isEmpty()) {
        m_currentFileName = fileName;
        SetDocumentModified(false);
        return true;
    }
    return false;
}

void MainWindow::OnExit() {
    close();
}

void MainWindow::OnUndo() {
    qDebug() << "=== OnUndo TRIGGERED ===";
    qDebug() << "OnUndo called - checking undo availability:" << m_ocafManager->CanUndo();
    if (m_ocafManager->Undo()) {
        qDebug() << "Undo operation successful, refreshing UI";
        // Refresh UI from OCAF document state
        RefreshUIFromOCAF();
        SetDocumentModified(true);
        UpdateActions();
        statusBar()->showMessage("Undo completed", 2000);
    } else {
        qDebug() << "Undo operation failed - available undos:" << m_ocafManager->CanUndo();
        statusBar()->showMessage("Cannot undo", 2000);
    }
}

void MainWindow::OnRedo() {
    qDebug() << "=== OnRedo TRIGGERED ===";
    qDebug() << "OnRedo called - checking redo availability:" << m_ocafManager->CanRedo();
    if (m_ocafManager->Redo()) {
        qDebug() << "Redo operation successful, refreshing UI";
        // Refresh UI from OCAF document state
        RefreshUIFromOCAF();
        SetDocumentModified(true);
        UpdateActions();
        statusBar()->showMessage("Redo completed", 2000);
    } else {
        qDebug() << "Redo operation failed - available redos:" << m_ocafManager->CanRedo();
        statusBar()->showMessage("Cannot redo", 2000);
    }
}

void MainWindow::OnFitAll() {
    m_viewer->FitAll();
    m_viewer->RedrawAll();
}

void MainWindow::OnZoomIn() {
    m_viewer->ZoomIn();
}

void MainWindow::OnZoomOut() {
    m_viewer->ZoomOut();
}

void MainWindow::OnViewWireframe() {
    m_viewer->SetViewMode("wireframe");
}

void MainWindow::OnViewShaded() {
    m_viewer->SetViewMode("shaded");
}

void MainWindow::OnViewOrthographic() {
    m_viewer->SetProjectionMode(true);
}

void MainWindow::OnViewPerspective() {
    m_viewer->SetProjectionMode(false);
}

void MainWindow::OnCreateBox() {
    CreateBoxDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        double width = dialog.GetWidth();
        double height = dialog.GetHeight();
        double depth = dialog.GetDepth();
        
        // Start OCAF transaction
        m_ocafManager->StartTransaction("Create Box");
        
        // Create the shape using ShapeFactory
        auto shape = cad_core::ShapeFactory::CreateBox(width, height, depth);
        if (shape) {
            // Add shape to OCAF document
            if (m_ocafManager->AddShape(shape, "Box")) {
                // Display the shape
                m_viewer->DisplayShape(shape);
                m_documentTree->AddShape(shape);
                
                // Commit the transaction
                m_ocafManager->CommitTransaction();
                SetDocumentModified(true);
                UpdateActions();
            } else {
                m_ocafManager->AbortTransaction();
                QMessageBox::warning(this, "Error", "Failed to add box to document.");
            }
        } else {
            m_ocafManager->AbortTransaction();
            QMessageBox::warning(this, "Error", "Failed to create box. Check parameters.");
        }
    }
}

void MainWindow::OnCreateCylinder() {
    CreateCylinderDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        double radius = dialog.GetRadius();
        double height = dialog.GetHeight();
        
        // Start OCAF transaction
        m_ocafManager->StartTransaction("Create Cylinder");
        
        // Create the shape using ShapeFactory
        auto shape = cad_core::ShapeFactory::CreateCylinder(radius, height);
        if (shape) {
            // Add shape to OCAF document
            if (m_ocafManager->AddShape(shape, "Cylinder")) {
                // Display the shape
                m_viewer->DisplayShape(shape);
                m_documentTree->AddShape(shape);
                
                // Commit the transaction
                m_ocafManager->CommitTransaction();
                SetDocumentModified(true);
                UpdateActions();
            } else {
                m_ocafManager->AbortTransaction();
                QMessageBox::warning(this, "Error", "Failed to add cylinder to document.");
            }
        } else {
            m_ocafManager->AbortTransaction();
            QMessageBox::warning(this, "Error", "Failed to create cylinder. Check parameters.");
        }
    }
}

void MainWindow::OnCreateSphere() {
    CreateSphereDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        double radius = dialog.GetRadius();
        
        // Start OCAF transaction
        m_ocafManager->StartTransaction("Create Sphere");
        
        // Create the shape using ShapeFactory
        auto shape = cad_core::ShapeFactory::CreateSphere(radius);
        if (shape) {
            // Add shape to OCAF document
            if (m_ocafManager->AddShape(shape, "Sphere")) {
                // Display the shape
                m_viewer->DisplayShape(shape);
                m_documentTree->AddShape(shape);
                
                // Commit the transaction
                m_ocafManager->CommitTransaction();
                SetDocumentModified(true);
                UpdateActions();
            } else {
                m_ocafManager->AbortTransaction();
                QMessageBox::warning(this, "Error", "Failed to add sphere to document.");
            }
        } else {
            m_ocafManager->AbortTransaction();
            QMessageBox::warning(this, "Error", "Failed to create sphere. Check parameters.");
        }
    }
}

void MainWindow::OnCreateTorus() {
    CreateTorusDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        double majorRadius = dialog.GetMajorRadius();
        double minorRadius = dialog.GetMinorRadius();

        m_ocafManager->StartTransaction("Create Torus");

        try {

            auto shape = cad_core::ShapeFactory::CreateTorus(cad_core::Point(0, 0, 0), majorRadius, minorRadius);

            if (m_ocafManager->AddShape(shape, "Torus")) {
                m_viewer->DisplayShape(shape);
                m_documentTree->AddShape(shape);

                m_ocafManager->CommitTransaction();
                SetDocumentModified(true);
                UpdateActions();
            }
            else {
                throw std::runtime_error("Failed to add torus to the document.");
            }
        }
        catch (const std::exception& e) {

            m_ocafManager->AbortTransaction(); 
            QMessageBox::warning(this, "Error Creating Torus", e.what());
        }
    }
}


void MainWindow::OnCreateExtrude() {
    // 1. 检查有没有可用的草图
    if (!m_lastCompletedSketch || m_lastCompletedSketch->IsEmpty()) {
        QMessageBox::warning(this, "拉伸错误", "没有可用于拉伸的草图。请先绘制一个封闭的草图并退出草图模式。");
        return;
    }

    // 2. 弹出一个对话框，让用户输入拉伸距离
    bool ok;
    double distance = QInputDialog::getDouble(this, "输入拉伸距离", "距离:", 10.0, 0.1, 1000.0, 2, &ok);

    if (!ok) {
        return; // 用户取消了输入
    }

    // 3. 创建一个 ExtrudeFeature 实例
    auto extrudeFeature = std::make_shared<cad_feature::ExtrudeFeature>();
    extrudeFeature->SetSketch(m_lastCompletedSketch);
    extrudeFeature->SetDistance(distance);

    // 4. 执行特征来创建3D形状
    cad_core::ShapePtr resultShape = extrudeFeature->CreateShape();

    // 5. 将新形状添加到文档并显示
    if (resultShape && resultShape->IsValid()) {
        m_ocafManager->StartTransaction("Extrude Sketch");
        m_ocafManager->AddShape(resultShape, "Extrusion");
        m_ocafManager->CommitTransaction();

        m_viewer->DisplayShape(resultShape);
        m_documentTree->AddShape(resultShape);

        // 操作完成后，禁用拉伸按钮，防止重复使用同一个草图
        m_createExtrudeAction->setEnabled(false);
        m_lastCompletedSketch.reset(); // 清空已使用的草图
    }
    else {
        QMessageBox::critical(this, "拉伸失败", "无法创建拉伸实体。请确保草图是封闭的。");
    }
}

void MainWindow::OnDarkTheme() {
    m_themeManager->SetTheme("dark");
}

void MainWindow::OnLightTheme() {
    m_themeManager->SetTheme("light");
}

void MainWindow::OnAbout() {
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::OnAboutQt() {
    QMessageBox::aboutQt(this);
}

void MainWindow::OnShapeSelected(const cad_core::ShapePtr& shape) {
    // Update property panel with selected shape
    m_propertyPanel->SetShape(shape);
    
    // Forward selection to active dialogs
    OnObjectSelected(shape);
}

void MainWindow::OnViewChanged() {
    // Handle view changes
}

// Document tree selection handlers
void MainWindow::OnDocumentTreeShapeSelected(const cad_core::ShapePtr& shape) {
    // When a shape is selected in the document tree, select it in the 3D viewer
    if (m_viewer && shape) {
        m_viewer->SelectShape(shape);
        m_propertyPanel->SetShape(shape);
    }
}

void MainWindow::OnDocumentTreeFeatureSelected(const cad_feature::FeaturePtr& feature) {
    // Handle feature selection from document tree
    if (feature) {
        // Update property panel to show feature properties
        // This would require extending PropertyPanel to handle features
        qDebug() << "Feature selected:" << QString::fromStdString(feature->GetName());
    }
}

// Missing slot implementations
void MainWindow::OnCut() {
    // Cut implementation placeholder
}

void MainWindow::OnCopy() {
    // Copy implementation placeholder
}

void MainWindow::OnPaste() {
    // Paste implementation placeholder
}

void MainWindow::OnDelete() {
    // Delete implementation placeholder
}

void MainWindow::OnSelectAll() {
    // Select all implementation placeholder
}

void MainWindow::OnCreateRevolve() {
    QMessageBox::information(this, "Create Revolve", "Revolve feature creation not implemented yet");
}

void MainWindow::OnCreateSweep() {
    QMessageBox::information(this, "Create Sweep", "Sweep feature creation not implemented yet");
}

void MainWindow::OnCreateLoft() {
    QMessageBox::information(this, "Create Loft", "Loft feature creation not implemented yet");
}

void MainWindow::OnImportSTEP() {
    QMessageBox::information(this, "Import STEP", "STEP import not implemented yet");
}

void MainWindow::OnImportIGES() {
    QMessageBox::information(this, "Import IGES", "IGES import not implemented yet");
}

void MainWindow::OnExportSTEP() {
    QMessageBox::information(this, "Export STEP", "STEP export not implemented yet");
}

void MainWindow::OnExportIGES() {
    QMessageBox::information(this, "Export IGES", "IGES export not implemented yet");
}

void MainWindow::OnExportSTL() {
    QMessageBox::information(this, "Export STL", "STL export not implemented yet");
}

void MainWindow::OnShowGrid() {
    // Toggle grid visibility
    static bool gridVisible = false;
    gridVisible = !gridVisible;
    m_viewer->ShowGrid(gridVisible);
}

void MainWindow::OnShowAxes() {
    // Toggle axes visibility
    static bool axesVisible = true;
    axesVisible = !axesVisible;
    m_viewer->ShowAxes(axesVisible);
}

void MainWindow::SetTheme(const QString& theme) {
    m_themeManager->SetTheme(theme);
}

// Boolean operations
void MainWindow::OnBooleanUnion() {
    // Clean up any existing dialog
    if (m_currentBooleanDialog) {
        m_currentBooleanDialog->deleteLater();
        m_currentBooleanDialog = nullptr;
    }
    
    // Create and show dialog
    m_currentBooleanDialog = new BooleanOperationDialog(BooleanOperationType::Union, this);
    
    // Connect dialog signals
    connect(m_currentBooleanDialog, &BooleanOperationDialog::selectionModeChanged,
            this, &MainWindow::OnSelectionModeChanged);
    connect(m_currentBooleanDialog, &BooleanOperationDialog::operationRequested,
            this, &MainWindow::OnBooleanOperationRequested);
    
    m_currentBooleanDialog->show();
    m_currentBooleanDialog->raise();
    m_currentBooleanDialog->activateWindow();
}

void MainWindow::OnBooleanIntersection() {
    // Clean up any existing dialog
    if (m_currentBooleanDialog) {
        m_currentBooleanDialog->deleteLater();
        m_currentBooleanDialog = nullptr;
    }
    
    // Create and show dialog
    m_currentBooleanDialog = new BooleanOperationDialog(BooleanOperationType::Intersection, this);
    
    // Connect dialog signals
    connect(m_currentBooleanDialog, &BooleanOperationDialog::selectionModeChanged,
            this, &MainWindow::OnSelectionModeChanged);
    connect(m_currentBooleanDialog, &BooleanOperationDialog::operationRequested,
            this, &MainWindow::OnBooleanOperationRequested);
    
    m_currentBooleanDialog->show();
    m_currentBooleanDialog->raise();
    m_currentBooleanDialog->activateWindow();
}

void MainWindow::OnBooleanDifference() {
    // Clean up any existing dialog
    if (m_currentBooleanDialog) {
        m_currentBooleanDialog->deleteLater();
        m_currentBooleanDialog = nullptr;
    }
    
    // Create and show dialog
    m_currentBooleanDialog = new BooleanOperationDialog(BooleanOperationType::Difference, this);
    
    // Connect dialog signals
    connect(m_currentBooleanDialog, &BooleanOperationDialog::selectionModeChanged,
            this, &MainWindow::OnSelectionModeChanged);
    connect(m_currentBooleanDialog, &BooleanOperationDialog::operationRequested,
            this, &MainWindow::OnBooleanOperationRequested);
    
    m_currentBooleanDialog->show();
    m_currentBooleanDialog->raise();
    m_currentBooleanDialog->activateWindow();
}

// Modify operations
void MainWindow::OnFillet() {
    // Clean up any existing dialog
    if (m_currentFilletChamferDialog) {
        m_currentFilletChamferDialog->deleteLater();
        m_currentFilletChamferDialog = nullptr;
    }
    
    // Create and show dialog
    m_currentFilletChamferDialog = new FilletChamferDialog(FilletChamferType::Fillet, m_viewer, this);
    
    // Connect dialog signals
    connect(m_currentFilletChamferDialog, &FilletChamferDialog::selectionModeChanged,
            this, &MainWindow::OnSelectionModeChanged);
    connect(m_currentFilletChamferDialog, &FilletChamferDialog::operationRequested,
            this, &MainWindow::OnFilletChamferOperationRequested);
    
    m_currentFilletChamferDialog->show();
    m_currentFilletChamferDialog->raise();
    m_currentFilletChamferDialog->activateWindow();
}

void MainWindow::OnChamfer() {
    // Clean up any existing dialog
    if (m_currentFilletChamferDialog) {
        m_currentFilletChamferDialog->deleteLater();
        m_currentFilletChamferDialog = nullptr;
    }
    
    // Create and show dialog
    m_currentFilletChamferDialog = new FilletChamferDialog(FilletChamferType::Chamfer, m_viewer, this);
    
    // Connect dialog signals
    connect(m_currentFilletChamferDialog, &FilletChamferDialog::selectionModeChanged,
            this, &MainWindow::OnSelectionModeChanged);
    connect(m_currentFilletChamferDialog, &FilletChamferDialog::operationRequested,
            this, &MainWindow::OnFilletChamferOperationRequested);
    
    m_currentFilletChamferDialog->show();
    m_currentFilletChamferDialog->raise();
    m_currentFilletChamferDialog->activateWindow();
}

// Selection mode combo box
void MainWindow::OnSelectionModeComboChanged(int index) {
    if (!m_selectionModeCombo) return;
    
    // Get the selection mode from combo box data
    cad_core::SelectionMode mode = static_cast<cad_core::SelectionMode>(
        m_selectionModeCombo->itemData(index).toInt());
    
    // Convert to OpenCASCADE selection mode integers
    int occSelectionMode = 0; // Default to shape
    switch (mode) {
        case cad_core::SelectionMode::Shape:
            occSelectionMode = 0;
            break;
        case cad_core::SelectionMode::Vertex:
            occSelectionMode = 1;
            break;
        case cad_core::SelectionMode::Edge:
            occSelectionMode = 2;
            break;
        case cad_core::SelectionMode::Face:
            occSelectionMode = 4;
            break;
    }
    
    // Set the selection mode in viewer
    m_viewer->SetSelectionMode(occSelectionMode);
    
    // Update status bar
    QString modeText = m_selectionModeCombo->itemText(index);
    statusBar()->showMessage("Selection Mode: " + modeText.replace("Select ", ""));
}

// Tab management
void MainWindow::CloseDocumentTab(int index) {
    if (m_tabWidget->count() <= 1) {
        return; // Keep at least one tab
    }
    
    QtOccView* viewer = qobject_cast<QtOccView*>(m_tabWidget->widget(index));
    if (viewer) {
        // Check for unsaved changes in this tab
        // For now, just close without checking
        m_tabWidget->removeTab(index);
        viewer->deleteLater();
    }
}

void MainWindow::OnTabChanged(int index) {
    if (index >= 0 && index < m_tabWidget->count()) {
        m_viewer = qobject_cast<QtOccView*>(m_tabWidget->widget(index));
        UpdateCurrentDocument();
    }
}

void MainWindow::NewDocumentTab() {
    QString tabName = QString("Document %1").arg(m_tabWidget->count() + 1);
    QtOccView* newViewer = new QtOccView(this);
    newViewer->setObjectName("viewer3D");
    newViewer->InitViewer();
    
    int tabIndex = m_tabWidget->addTab(newViewer, tabName);
    m_tabWidget->setCurrentIndex(tabIndex);
    
    // Connect viewer signals for new tab
    connect(newViewer, &QtOccView::ShapeSelected, this, &MainWindow::OnShapeSelected);
    connect(newViewer, &QtOccView::ViewChanged, this, &MainWindow::OnViewChanged);
}

QtOccView* MainWindow::GetCurrentViewer() const {
    if (m_tabWidget && m_tabWidget->currentIndex() >= 0) {
        return qobject_cast<QtOccView*>(m_tabWidget->currentWidget());
    }
    return nullptr;
}

void MainWindow::UpdateCurrentDocument() {
    m_viewer = GetCurrentViewer();
    UpdateActions();
    UpdateWindowTitle();
}

// Window dragging implementation for frameless window
void MainWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // Check if click is in the title bar area
        QWidget* widget = childAt(event->pos());
        if (widget == m_titleBar || widget == m_titleLabel) {
            m_isDragging = true;
            m_dragStartPosition = event->globalPos() - frameGeometry().topLeft();
            event->accept();
            return;
        }
    }
    QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton && m_isDragging) {
        move(event->globalPos() - m_dragStartPosition);
        event->accept();
        return;
    }
    QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        event->accept();
        return;
    }
    QMainWindow::mouseReleaseEvent(event);
}

// Create custom title bar for frameless window
void MainWindow::CreateTitleBar() {
    m_titleBar = new QWidget(this);
    m_titleBar->setObjectName("titleBar");
    m_titleBar->setFixedHeight(40);
    
    // Create title label
    m_titleLabel = new QLabel("Ander CAD", m_titleBar);
    m_titleLabel->setObjectName("titleLabel");
    
    // Create window control buttons
	// 最小化按钮、最大化按钮、关闭按钮
    m_minimizeButton = new QPushButton("−", m_titleBar);
    m_minimizeButton->setObjectName("minimizeButton");
    m_minimizeButton->setFixedSize(40, 40);
    m_minimizeButton->setToolTip("Minimize");
    
    m_maximizeButton = new QPushButton("□", m_titleBar);
    m_maximizeButton->setObjectName("maximizeButton");
    m_maximizeButton->setFixedSize(40, 40);
    m_maximizeButton->setToolTip("Maximize");
    
    m_closeButton = new QPushButton("✕", m_titleBar);
    m_closeButton->setObjectName("closeButton");
    m_closeButton->setFixedSize(40, 40);
    m_closeButton->setToolTip("Close");
    
    
    // Layout for title bar
    QHBoxLayout* titleLayout = new QHBoxLayout(m_titleBar);
    titleLayout->setContentsMargins(8, 0, 0, 0);
    titleLayout->setSpacing(0);
    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(m_minimizeButton);
    titleLayout->addWidget(m_maximizeButton);
    titleLayout->addWidget(m_closeButton);
    
    // Connect buttons to slots
    connect(m_minimizeButton, &QPushButton::clicked, this, &MainWindow::OnMinimizeWindow);
    connect(m_maximizeButton, &QPushButton::clicked, this, &MainWindow::OnMaximizeWindow);
    connect(m_closeButton, &QPushButton::clicked, this, &MainWindow::OnCloseWindow);
    
    // Position title bar at the top of the window
    m_titleBar->setParent(this);
    m_titleBar->move(0, 0);
    m_titleBar->resize(width(), 40);
}

// Window control slots
void MainWindow::OnMinimizeWindow() {
    showMinimized();
}

void MainWindow::OnMaximizeWindow() {
    if (isMaximized()) {
        showNormal();
        m_maximizeButton->setText("□");
        m_maximizeButton->setToolTip("Maximize");
    } else {
        showMaximized();
        m_maximizeButton->setText("◱");
        m_maximizeButton->setToolTip("Restore");
    }
}

void MainWindow::OnCloseWindow() {
    close();
}

void MainWindow::CreateSelectionModeCombo() {
    // Create the combo box
    m_selectionModeCombo = new QComboBox(this);
    m_selectionModeCombo->addItem("Select Shape", static_cast<int>(cad_core::SelectionMode::Shape));
    m_selectionModeCombo->addItem("Select Face", static_cast<int>(cad_core::SelectionMode::Face));
    m_selectionModeCombo->addItem("Select Edge", static_cast<int>(cad_core::SelectionMode::Edge));
    m_selectionModeCombo->addItem("Select Vertex", static_cast<int>(cad_core::SelectionMode::Vertex));
    
    // Set default selection
    m_selectionModeCombo->setCurrentIndex(0); // Shape mode by default
    
    // Connect signal
    connect(m_selectionModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::OnSelectionModeComboChanged);
}

void MainWindow::CreateConsole() {
    // Create console output text edit
    m_console = new QTextEdit(this);
    m_console->setObjectName("console");
    m_console->setMaximumHeight(200);
    m_console->setMinimumHeight(100);
    m_console->setReadOnly(true);
    m_console->setStyleSheet(
        "QTextEdit {"
        "   background-color: #1e1e1e;"
        "   color: #ffffff;"
        "   font-family: 'Consolas', 'Monaco', monospace;"
        "   font-size: 9pt;"
        "   border: 1px solid #3c3c3c;"
        "}"
    );
    
    // Install custom message handler to redirect qDebug to console
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &context, const QString &msg) {
        // Get the main window instance to access console
        foreach (QWidget *widget, QApplication::topLevelWidgets()) {
            MainWindow *mainWindow = qobject_cast<MainWindow*>(widget);
            if (mainWindow && mainWindow->m_console) {
                QString formattedMsg;
                switch (type) {
                    case QtDebugMsg:
                        formattedMsg = QString("[DEBUG] %1").arg(msg);
                        break;
                    case QtWarningMsg:
                        formattedMsg = QString("[WARNING] %1").arg(msg);
                        break;
                    case QtCriticalMsg:
                        formattedMsg = QString("[CRITICAL] %1").arg(msg);
                        break;
                    case QtFatalMsg:
                        formattedMsg = QString("[FATAL] %1").arg(msg);
                        break;
                    case QtInfoMsg:
                        formattedMsg = QString("[INFO] %1").arg(msg);
                        break;
                }
                mainWindow->m_console->append(formattedMsg);
                break;
            }
        }
    });
    
    m_console->append("[SYSTEM] Console initialized");
}

// Dialog interaction slots
void MainWindow::OnSelectionModeChanged(bool enabled, const QString& prompt) {
    if (enabled) {
        // Enable 3D selection mode
        statusBar()->showMessage(prompt);
        
        // Determine selection mode based on active dialog
        if (m_currentFilletChamferDialog) {
            // For fillet/chamfer operations, switch to edge selection mode
            m_viewer->SetSelectionMode(2); // Edge mode (OpenCASCADE mode 2)
            m_viewer->ClearEdgeSelection(); // Clear previous edge selections
            
            // Update combo box selection
            if (m_selectionModeCombo) {
                m_selectionModeCombo->setCurrentIndex(2); // Edge mode
            }
        } else if (m_currentBooleanDialog) {
            // For boolean operations, use shape selection mode
            m_viewer->SetSelectionMode(cad_core::SelectionMode::Shape);
            
            // Update combo box selection
            if (m_selectionModeCombo) {
                m_selectionModeCombo->setCurrentIndex(0); // Shape mode
            }
        } else {
            // Default to shape selection
            m_viewer->SetSelectionMode(cad_core::SelectionMode::Shape);
            
            // Update combo box selection
            if (m_selectionModeCombo) {
                m_selectionModeCombo->setCurrentIndex(0); // Shape mode
            }
        }
    } else {
        // Disable selection mode
        statusBar()->showMessage("Ready");
        // Return to default shape selection mode
        m_viewer->SetSelectionMode(cad_core::SelectionMode::Shape);
    }
}

void MainWindow::OnObjectSelected(const cad_core::ShapePtr& shape) {
    // Forward selection to the active dialog
    if (m_currentBooleanDialog) {
        m_currentBooleanDialog->onObjectSelected(shape);
    }
    if (m_currentFilletChamferDialog) {
        m_currentFilletChamferDialog->onEdgeSelected(shape);
    }
    if (m_currentTransformDialog) {
        m_currentTransformDialog->onObjectSelected(shape);
    }
}

void MainWindow::OnBooleanOperationRequested(BooleanOperationType type, 
                                           const std::vector<cad_core::ShapePtr>& targets,
                                           const std::vector<cad_core::ShapePtr>& tools) {
    // Validate selection based on operation type
    if (type == BooleanOperationType::Union) {
        if (targets.empty()) {
            QMessageBox::warning(this, "Boolean Union", "Please select multiple objects to merge.");
            return;
        }
        if (targets.size() < 2 && tools.empty()) {
            QMessageBox::warning(this, "Boolean Union", "Please select at least 2 objects to merge.");
            return;
        }
    } else {
        if (targets.empty() || tools.empty()) {
            QMessageBox::warning(this, "Boolean Operation", "Please select both target and tool objects.");
            return;
        }
    }
    
    // Start OCAF transaction
    QString operationName;
    switch (type) {
        case BooleanOperationType::Union:
            operationName = "Boolean Union";
            break;
        case BooleanOperationType::Intersection:
            operationName = "Boolean Intersection";
            break;
        case BooleanOperationType::Difference:
            operationName = "Boolean Difference";
            break;
    }
    
    m_ocafManager->StartTransaction(operationName.toStdString());
    
    cad_core::ShapePtr result;
    try {
        if (type == BooleanOperationType::Union) {
            // Combine all targets and tools for union
            std::vector<cad_core::ShapePtr> allShapes = targets;
            allShapes.insert(allShapes.end(), tools.begin(), tools.end());
            result = cad_core::BooleanOperations::Union(allShapes);
        } else if (type == BooleanOperationType::Intersection) {
            // Use first target as base, intersect with all others
            result = targets[0];
            for (size_t i = 1; i < targets.size(); ++i) {
                if (result) {
                    result = cad_core::BooleanOperations::Intersection({result, targets[i]});
                }
            }
            for (const auto& tool : tools) {
                if (result) {
                    result = cad_core::BooleanOperations::Intersection({result, tool});
                }
            }
        } else if (type == BooleanOperationType::Difference) {
            // Use first target as base, subtract all tools
            result = targets[0];
            for (const auto& tool : tools) {
                if (result) {
                    result = cad_core::BooleanOperations::Difference(result, tool);
                }
            }
        }
        
        if (result) {
            // Add result to document
            if (m_ocafManager->AddShape(result, (operationName + " Result").toStdString())) {
                // Display the new result shape
                m_viewer->DisplayShape(result);
                m_documentTree->AddShape(result);
                
                // For Union: Remove all original objects (targets and tools) from OCAF
                // For Intersection/Difference: Remove original and tool objects from OCAF, keep only result
                if (type == BooleanOperationType::Union) {
                    // Union: Remove all input objects (targets + tools) from OCAF document
                    for (const auto& shape : targets) {
                        m_ocafManager->RemoveShape(shape);  // Remove from OCAF
                        m_viewer->RemoveShape(shape);       // Remove from 3D view
                        m_documentTree->RemoveShape(shape); // Remove from document tree
                    }
                    for (const auto& shape : tools) {
                        m_ocafManager->RemoveShape(shape);  // Remove from OCAF
                        m_viewer->RemoveShape(shape);       // Remove from 3D view
                        m_documentTree->RemoveShape(shape); // Remove from document tree
                    }
                } else {
                    // Intersection/Difference: Remove original and tool objects from OCAF
                    for (const auto& shape : targets) {
                        m_ocafManager->RemoveShape(shape);  // Remove from OCAF
                        m_viewer->RemoveShape(shape);       // Remove from 3D view
                        m_documentTree->RemoveShape(shape); // Remove from document tree
                    }
                    for (const auto& shape : tools) {
                        m_ocafManager->RemoveShape(shape);  // Remove from OCAF
                        m_viewer->RemoveShape(shape);       // Remove from 3D view
                        m_documentTree->RemoveShape(shape); // Remove from document tree
                    }
                }
                
                m_ocafManager->CommitTransaction();
                SetDocumentModified(true);
                UpdateActions();
                statusBar()->showMessage(operationName + " completed successfully");
            } else {
                m_ocafManager->AbortTransaction();
                QMessageBox::warning(this, "Error", "Failed to add result to document.");
            }
        } else {
            m_ocafManager->AbortTransaction();
            QMessageBox::warning(this, "Error", operationName + " operation failed.");
        }
    } catch (const std::exception& e) {
        m_ocafManager->AbortTransaction();
        QMessageBox::warning(this, "Error", QString("Boolean operation failed: %1").arg(e.what()));
    }
    
    // Clean up dialog
    if (m_currentBooleanDialog) {
        m_currentBooleanDialog->deleteLater();
        m_currentBooleanDialog = nullptr;
    }
}

void MainWindow::OnFilletChamferOperationRequested(FilletChamferType type, 
                                                 const std::vector<cad_core::ShapePtr>& edges,
                                                 double radius, double distance1, double distance2) {
    if (edges.empty()) {
        QMessageBox::warning(this, "Fillet/Chamfer", "Please select edges for operation.");
        return;
    }
    
    // Get selected edges grouped by their parent shapes
    auto edgesByShape = m_viewer->GetSelectedEdgesByShape();
    if (edgesByShape.empty()) {
        QMessageBox::warning(this, "Fillet/Chamfer", "No edges selected in 3D view. Please select edges first.");
        return;
    }
    
    qDebug() << "Fillet/Chamfer operation requested with edges from" << edgesByShape.size() << "shape(s)";
    
    // Start OCAF transaction
    QString operationName = (type == FilletChamferType::Fillet) ? "Fillet" : "Chamfer";
    m_ocafManager->StartTransaction(operationName.toStdString());
    
    try {
        bool anySuccess = false;
        
        // Process each shape that has selected edges
        for (const auto& shapeEdgePair : edgesByShape) {
            cad_core::ShapePtr baseShape = shapeEdgePair.first;
            const std::vector<TopoDS_Edge>& edges = shapeEdgePair.second;
            
            if (!baseShape || edges.empty()) {
                continue;
            }
            
            qDebug() << "Processing" << edges.size() << "edges on shape";
            
            // Perform the operation on this shape with its edges
            cad_core::ShapePtr result;
            if (type == FilletChamferType::Fillet) {
                result = cad_core::FilletChamferOperations::CreateFillet(baseShape, edges, radius);
            } else {
                result = cad_core::FilletChamferOperations::CreateChamfer(baseShape, edges, distance1);
            }
            
            if (result) {
                QString shapeName = QString("%1 Result on Shape").arg(operationName);
                if (m_ocafManager->AddShape(result, shapeName.toStdString())) {
                    // Remove the original shape from OCAF, viewer, and document tree
                    qDebug() << "Removing original shape before displaying" << operationName << "result";
                    m_ocafManager->RemoveShape(baseShape);  // Remove from OCAF
                    m_viewer->RemoveShape(baseShape);       // Remove from 3D view
                    m_documentTree->RemoveShape(baseShape); // Remove from document tree
                    
                    // Display the new result
                    m_viewer->DisplayShape(result);
                    m_documentTree->AddShape(result);
                    anySuccess = true;
                    qDebug() << "Successfully created" << operationName << "with" << edges.size() << "edges";
                } else {
                    qDebug() << "Failed to add" << operationName << "result to OCAF";
                }
            } else {
                qDebug() << operationName << "operation failed for this shape";
            }
        }
        
        if (anySuccess) {
            m_ocafManager->CommitTransaction();
            SetDocumentModified(true);
            UpdateActions();
            statusBar()->showMessage(operationName + " completed successfully");
        } else {
            m_ocafManager->AbortTransaction();
            QMessageBox::warning(this, "Error", operationName + " operation failed.");
        }
    } catch (const std::exception& e) {
        m_ocafManager->AbortTransaction();
        QMessageBox::warning(this, "Error", QString("%1 operation failed: %2").arg(operationName).arg(e.what()));
    }
    
    // Clear edge selection after operation
    m_viewer->ClearEdgeSelection();
    
    // Clean up dialog
    if (m_currentFilletChamferDialog) {
        m_currentFilletChamferDialog->deleteLater();
        m_currentFilletChamferDialog = nullptr;
    }
}

// =============================================================================
// Transform Operations Implementation
// =============================================================================

void MainWindow::OnTransformObjects() {
    if (m_currentTransformDialog) {
        m_currentTransformDialog->raise();
        m_currentTransformDialog->activateWindow();
        return;
    }
    
    m_currentTransformDialog = new TransformOperationDialog(this);
    
    // Connect dialog signals
    connect(m_currentTransformDialog, &TransformOperationDialog::selectionModeChanged,
            this, &MainWindow::OnSelectionModeChanged);
    connect(m_currentTransformDialog, &TransformOperationDialog::transformRequested,
            this, &MainWindow::OnTransformOperationRequested);
    connect(m_currentTransformDialog, &TransformOperationDialog::previewRequested,
            this, &MainWindow::OnTransformPreviewRequested);
    connect(m_currentTransformDialog, &TransformOperationDialog::resetRequested,
            this, &MainWindow::OnTransformResetRequested);
    
    // Show dialog
    m_currentTransformDialog->show();
}

void MainWindow::OnTransformOperationRequested(std::shared_ptr<cad_core::TransformCommand> command) {
    if (!command) {
        return;
    }
    
    try {
        // Reset any preview first
        if (m_previewActive) {
            OnTransformResetRequested();
        }
        
        // Execute the transform command to get transformed shapes
        if (command->Execute()) {
            // Get original and transformed shapes
            auto originalShapes = m_currentTransformDialog->getSelectedObjects();
            auto transformedShapes = command->GetTransformedShapes();
            
            // Start OCAF transaction
            m_ocafManager->StartTransaction("Transform Objects");
            
            // Replace shapes in OCAF document
            for (size_t i = 0; i < originalShapes.size() && i < transformedShapes.size(); ++i) {
                if (m_ocafManager->ReplaceShape(originalShapes[i], transformedShapes[i])) {
                    // Update display
                    m_viewer->RemoveShape(originalShapes[i]);
                    m_viewer->DisplayShape(transformedShapes[i]);
                    
                    // Update document tree
                    m_documentTree->RemoveShape(originalShapes[i]);
                    m_documentTree->AddShape(transformedShapes[i]);
                } else {
                    m_ocafManager->AbortTransaction();
                    QMessageBox::warning(this, "错误", "无法更新形状");
                    return;
                }
            }
            
            // Commit transaction
            m_ocafManager->CommitTransaction();
            
            // Update display and mark as modified
            RefreshUIFromOCAF();
            SetDocumentModified(true);
            
            // Update status bar
            statusBar()->showMessage(QString("变换操作完成: %1").arg(command->GetName()), 0.5);
        } else {
            QMessageBox::warning(this, "错误", "变换操作执行失败");
        }
    } catch (const std::exception& e) {
        m_ocafManager->AbortTransaction();
        QMessageBox::warning(this, "错误", QString("变换操作失败: %1").arg(e.what()));
    }
    
    // Clean up dialog
    if (m_currentTransformDialog) {
        m_currentTransformDialog->deleteLater();
        m_currentTransformDialog = nullptr;
    }
}

void MainWindow::OnTransformPreviewRequested(std::shared_ptr<cad_core::TransformCommand> command) {
    if (!command) {
        return;
    }
    
    try {
        // Clear any existing preview
        if (m_previewActive) {
            OnTransformResetRequested();
        }
        
        // Get preview shapes from command
        auto previewShapes = command->GetTransformedShapes();
        
        if (!previewShapes.empty()) {
            // Store preview shapes
            m_previewShapes = previewShapes;
            m_previewActive = true;
            
            // Display preview shapes with a different color/style
            for (const auto& shape : m_previewShapes) {
                if (shape && shape->IsValid()) {
                    // TODO: Set preview material/color (semi-transparent or different color)
                    m_viewer->DisplayShape(shape);
                }
            }
            
            // Update display
            m_viewer->update();
        }
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "错误", QString("预览生成失败: %1").arg(e.what()));
    }
}

void MainWindow::OnTransformResetRequested() {
    if (!m_previewActive) {
        return;
    }
    
    // Remove preview shapes from display
    for (const auto& shape : m_previewShapes) {
        if (shape) {
            m_viewer->RemoveShape(shape);
        }
    }
    
    // Clear preview data
    m_previewShapes.clear();
    m_previewActive = false;
    
    // Update display
    m_viewer->update();
}

// =============================================================================
// Sketch Mode Implementation
// =============================================================================

void MainWindow::OnEnterSketchMode() {
    if (!m_viewer) {
        qDebug() << "Error: No viewer available";
        return;
    }
    
    try {
        // 检查是否已经在草图模式中
        if (m_viewer->IsInSketchMode()) {
            qDebug() << "Already in sketch mode";
            return;
        }
        
        // 检查是否有可用的对象
        auto shapes = m_ocafManager->GetAllShapes();
        if (shapes.empty()) {
            if (m_statusBar) {
                statusBar()->showMessage("请先创建一个几何体（如盒子），然后选择一个面进入草图模式");
            }
            qDebug() << "No shapes available for face selection";
            return;
        }
        
        // 创建并显示面选择对话框
        FaceSelectionDialog* dialog = new FaceSelectionDialog(m_viewer, this);
        
        // 连接对话框信号
        connect(dialog, &FaceSelectionDialog::faceSelected, this, [this, dialog](const TopoDS_Face& face) {
            OnFaceSelectedForSketch(face);
            dialog->close();
            dialog->deleteLater();
        });
        
        connect(dialog, &FaceSelectionDialog::selectionCancelled, this, [this, dialog]() {
            if (m_statusBar) {
                statusBar()->showMessage("草图模式已取消");
            }
            dialog->close();
            dialog->deleteLater();
        });
        
        // 显示对话框 (非模态)
        dialog->show();
        
        qDebug() << "Face selection dialog shown";
    }
    catch (const std::exception& e) {
        qDebug() << "Error in OnEnterSketchMode:" << e.what();
        if (m_statusBar) {
            statusBar()->showMessage("进入草图模式失败");
        }
    }
}

void MainWindow::OnExitSketchMode() {
    if (!m_viewer || !m_viewer->IsInSketchMode()) {
        return;
    }

    // 保存当前草图
    m_lastCompletedSketch = m_viewer->GetCurrentSketch();
    m_viewer->ExitSketchMode();

    // 如果草图有效，则启用拉伸按钮
    if (m_lastCompletedSketch && !m_lastCompletedSketch->IsEmpty()) {
        m_createExtrudeAction->setEnabled(true);
    }
}

void MainWindow::OnSketchRectangleTool() {
    if (!m_viewer || !m_viewer->IsInSketchMode()) {
        return;
    }
    
    m_viewer->StartRectangleTool();
    statusBar()->showMessage("矩形工具已激活 - 点击并拖拽创建矩形");
}

void MainWindow::OnSketchLineTool() {
    if (!m_viewer || !m_viewer->IsInSketchMode()) {
        return;
    }

    m_viewer->StartLineTool();
    statusBar()->showMessage("直线工具已激活 - 点击并拖拽创建直线");
}

void MainWindow::OnSketchCircleTool() {
    if (!m_viewer || !m_viewer->IsInSketchMode()) {
        return;
    }

    m_viewer->StartCircleTool();
    statusBar()->showMessage("圆形工具已激活 - 点击确定圆心，拖拽确定半径");
}

void MainWindow::OnFaceSelected(const TopoDS_Face& face) {
    if (!m_waitingForFaceSelection) {
        return;
    }
    
    try {
        m_waitingForFaceSelection = false;
        m_selectedFace = face;
        
        // 检查面是否有效
        if (face.IsNull()) {
            qDebug() << "Error: Selected face is null";
            if (m_statusBar) {
                statusBar()->showMessage("选择的面无效");
            }
            return;
        }
        
        // Enter sketch mode with the selected face
        if (m_viewer) {
            m_viewer->EnterSketchMode(face);
        } else {
            qDebug() << "Error: No viewer available for sketch mode";
        }
        
        qDebug() << "Face selected, entering sketch mode";
    }
    catch (const std::exception& e) {
        qDebug() << "Error in OnFaceSelected:" << e.what();
        if (m_statusBar) {
            statusBar()->showMessage(QString("进入草图模式失败: %1").arg(e.what()));
        }
        m_waitingForFaceSelection = false;
    }
}

void MainWindow::OnFaceSelectedForSketch(const TopoDS_Face& face) {
    try {
        // 检查面是否有效
        if (face.IsNull()) {
            qDebug() << "Error: Selected face is null";
            if (m_statusBar) {
                statusBar()->showMessage("选择的面无效");
            }
            return;
        }
        
        // 直接进入草图模式
        if (m_viewer) {
            m_viewer->EnterSketchMode(face);
            if (m_statusBar) {
                statusBar()->showMessage("正在进入草图模式...");
            }
        } else {
            qDebug() << "Error: No viewer available for sketch mode";
            if (m_statusBar) {
                statusBar()->showMessage("视图不可用");
            }
        }
        
        qDebug() << "Face selected from dialog, entering sketch mode";
    }
    catch (const std::exception& e) {
        qDebug() << "Error in OnFaceSelectedForSketch:" << e.what();
        if (m_statusBar) {
            statusBar()->showMessage(QString("进入草图模式失败: %1").arg(e.what()));
        }
    }
}

void MainWindow::OnSketchModeEntered() {
    // Update UI state when sketch mode is entered
    m_enterSketchAction->setEnabled(false);
    m_exitSketchAction->setEnabled(true);
    m_sketchRectangleAction->setEnabled(true);
    m_sketchLineAction->setEnabled(true);
	m_sketchCircleAction->setEnabled(true);
    
    // Reset selection mode
    m_viewer->SetSelectionMode(0);  // Shape selection mode
    
    statusBar()->showMessage(QString("已进入草图模式 - 选择绘制工具开始绘制"));
    
    qDebug() << "Sketch mode entered, UI updated";
}

void MainWindow::OnSketchModeExited() {
    // Update UI state when sketch mode is exited
    m_enterSketchAction->setEnabled(true);
    m_exitSketchAction->setEnabled(false);
    m_sketchRectangleAction->setEnabled(false);
    m_sketchLineAction->setEnabled(false);
	m_sketchCircleAction->setEnabled(false);
    
    // Reset any waiting states
    m_waitingForFaceSelection = false;
    
    statusBar()->showMessage("已退出草图模式");
    
    qDebug() << "Sketch mode exited, UI updated";
}

} // namespace cad_ui

#include "MainWindow.moc"