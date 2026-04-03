#include "MainWindow.h"
#include "Loader/LoaderFactory.h"
#include "TestTool/Profiler.h"
#include "TestTool/debug.h"
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QLabel>
#include <QMenuBar>
#include <QSet>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QWidgetAction>
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("CAE Renderer");
    resize(1200, 800);
    setupUI();
    setupMenus();
}

void MainWindow::setupUI()
{
    /*GLWidget Begin*/
    gl_widget_ = new GLWidget(this);
    setCentralWidget(gl_widget_);
    /*GLWidget End*/


    /*Basic Render Options Dock Begin*/
    render_dock_ = new QDockWidget("Render Options", this);
    render_dock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    render_options_widget_ = new RenderOptionsWidget(this);
    render_dock_->setWidget(render_options_widget_);
    addDockWidget(Qt::LeftDockWidgetArea, render_dock_);
    // 连接场量选择信号
    connect(render_options_widget_->fieldCombo(),
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onFieldSelectionChanged);
    // 连接基础网格渲染模式
    connect(render_options_widget_->meshRenderModeCombo(),
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onMeshRenderModeChanged);
    // 连接LOD复选框
    connect(render_options_widget_->lodCheckBox(), &QCheckBox::toggled, this,
            &MainWindow::onLodCheckBoxToggled);
        connect(render_options_widget_->lodLevelCombo(),
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onLodLevelComboChanged);
    // 连接配色方案
    connect(render_options_widget_->colorSchemeCombo(),
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onColorSchemeChanged);
    // 连接向量渲染模式
    connect(render_options_widget_->vectorRenderModeCombo(),
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onVectorRenderModeChanged);
    /*Basic Render Options Dock End*/


    /* Streamline Options Dock Begin */
    streamline_dock_ = new QDockWidget("Streamline Options", this);
    streamline_dock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    streamline_options_widget_ = new StreamlineOptionsWidget(gl_widget_, this);
    streamline_dock_->setWidget(streamline_options_widget_);
    addDockWidget(Qt::LeftDockWidgetArea, streamline_dock_);
    streamline_dock_->setVisible(false); // 默认隐藏，用户可以通过菜单显示
    // 连接流线选项信号
    connect(streamline_options_widget_, &StreamlineOptionsWidget::streamlineVectorFieldChanged,
            this, &MainWindow::onStreamlineVectorFieldChanged);
    connect(streamline_options_widget_, &StreamlineOptionsWidget::streamlineRenderModeChanged, this,
            &MainWindow::onStreamlineRenderModeChanged);
        connect(streamline_options_widget_, &StreamlineOptionsWidget::streamlineLodEnabledChanged,
            this, &MainWindow::onStreamlineLodEnabledChanged);
    connect(streamline_options_widget_, &StreamlineOptionsWidget::streamlineLodLevelChanged, this,
            &MainWindow::onStreamlineLodLevelChanged);
    connect(streamline_options_widget_, &StreamlineOptionsWidget::seedSphereRadiusChanged, this,
            &MainWindow::onSeedSphereRadiusChanged);
    connect(streamline_options_widget_, &StreamlineOptionsWidget::seedSphereCountChanged, this,
            &MainWindow::onSeedSphereCountChanged);
    connect(streamline_options_widget_, &StreamlineOptionsWidget::seedSphereOffsetChanged, this,
            &MainWindow::onSeedSphereOffsetChanged);
    connect(streamline_options_widget_, &StreamlineOptionsWidget::generateStreamlinesRequested,
            this, &MainWindow::onGenerateStreamlinesRequested);
    /* Streamline Options Dock End */


    /* Properties Dock Begin */
    properties_dock_ = new QDockWidget("Properties", this);
    properties_dock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    properties_panel_widget_ = new PropertiesPanelWidget(this);
    properties_dock_->setWidget(properties_panel_widget_);
    addDockWidget(Qt::RightDockWidgetArea, properties_dock_);
    // 连接GLWidget的信号
    connect(gl_widget_, &GLWidget::fpsUpdated, this, &MainWindow::onFpsUpdated);
    connect(gl_widget_, &GLWidget::lodLevelChanged, this, &MainWindow::onLodLevelChanged);
    connect(gl_widget_, &GLWidget::seedSphereCenterChanged, this,
            &MainWindow::onSeedSphereCenterChanged);
    /* Properties Dock End */

    /* Status Bar Begin */
    fps_label_ = new QLabel("FPS: 0.0", this);
    fps_label_->setAlignment(Qt::AlignLeft);
    fps_label_->setMinimumWidth(100);
    statusBar()->addWidget(fps_label_);

    lod_label_ = new QLabel("LOD: HIGH", this);
    lod_label_->setAlignment(Qt::AlignLeft);
    lod_label_->setMinimumWidth(100);
    statusBar()->addWidget(lod_label_);
    /* Status Bar End */
}

void MainWindow::setupMenus()
{
    QMenu* file = menuBar()->addMenu("&File");
    openfile_ = file->addAction("Open &File...");
    opendir_ = file->addAction("Open &Directory...");
    connect(openfile_, &QAction::triggered, this, &MainWindow::openFile);
    connect(opendir_, &QAction::triggered, this, &MainWindow::openDirectory);

    QMenu* view = menuBar()->addMenu("&View");
    QAction* showRenderOptionsAction = view->addAction("Render Options");
    showRenderOptionsAction->setCheckable(true);
    showRenderOptionsAction->setChecked(render_dock_->isVisible());
    QAction* showStreamlineOptionsAction = view->addAction("Streamline Options");
    showStreamlineOptionsAction->setCheckable(true);
    showStreamlineOptionsAction->setChecked(streamline_dock_->isVisible());
    QAction* showPropertiesAction = view->addAction("Properties");
    showPropertiesAction->setCheckable(true);
    showPropertiesAction->setChecked(properties_dock_->isVisible());
    connect(showRenderOptionsAction, &QAction::toggled, this,
            [this](bool checked) { render_dock_->setVisible(checked); });
    connect(showStreamlineOptionsAction, &QAction::toggled, this,
            [this](bool checked) { streamline_dock_->setVisible(checked); });
    connect(showPropertiesAction, &QAction::toggled, this,
            [this](bool checked) { properties_dock_->setVisible(checked); });
    connect(render_dock_, &QDockWidget::visibilityChanged, this,
            [showRenderOptionsAction](bool visible)
            {
                QSignalBlocker blocker1(showRenderOptionsAction);
                showRenderOptionsAction->setChecked(visible);
            });
    connect(streamline_dock_, &QDockWidget::visibilityChanged, this,
            [showStreamlineOptionsAction](bool visible)
            {
                QSignalBlocker blocker2_streamline(showStreamlineOptionsAction);
                showStreamlineOptionsAction->setChecked(visible);
            });
    connect(properties_dock_, &QDockWidget::visibilityChanged, this,
            [showPropertiesAction](bool visible)
            {
                QSignalBlocker blocker2(showPropertiesAction);
                showPropertiesAction->setChecked(visible);
            });

    QMenu* display = menuBar()->addMenu("&Display");
    QAction* showMeshAction = display->addAction("Show Entity Model");
    showMeshAction->setCheckable(true);
    showMeshAction->setChecked(true);
    QAction* showStreamlineAction = display->addAction("Show Streamlines");
    showStreamlineAction->setCheckable(true);
    showStreamlineAction->setChecked(true);
    connect(showMeshAction, &QAction::toggled, this,
            [this](bool checked) { gl_widget_->setMeshVisible(checked); });
    connect(showStreamlineAction, &QAction::toggled, this,
            [this](bool checked) { gl_widget_->setStreamlineVisible(checked); });
}
void MainWindow::test_openFile()
{
    openFile();
}
void MainWindow::openFile()
{
    
#if DEBUG_MODE
    // QString filename = "E:/data/CAE/VTK/motorBike_500.vtk";
    QString filename = "E:\\data\\CAE\\VTK_Submarine2/Submarine_case_4.vtk";
#else
    QString filename = QFileDialog::getOpenFileName(
        this, "打开VTK文件", "E:/data/CAE", "VTK Files (*.vtk *.vtu *.vtp);;All Files (*.*);");
#endif
    if (filename.isEmpty())
    {
        qWarning() << "[MainWindow::openFile] :  No file selected.";
        return;
    }

    current_file_path_ = filename.toStdString();
    gl_widget_->loadFile(filename.toStdString());
    updatePropertiesPanel();
    updateFieldList();
}
void MainWindow::updatePropertiesPanel()
{
    properties_panel_widget_->setFilePathText(QString::fromStdString(current_file_path_));

    const CaseData* case_data = gl_widget_->getCaseData();
    if (!case_data || case_data->steps_.empty())
    {
        properties_panel_widget_->setInfoText("No data loaded.");
        return;
    }

    const TimeStepData& time_step = case_data->steps_[0];

    // 统计点、单元、场量信息
    int total_points = 0;
    int total_faces = 0;
    int total_point_fields = 0;
    int total_cell_fields = 0;

    for (const auto& part : time_step.parts_)
    {
        total_points += part.vertices_.size() / 3; // 3个浮点数为一个点
        total_faces += part.faces_.size();
        total_point_fields += part.point_fields_.size();
        total_cell_fields += part.cell_fields_.size();
    }

    QString text;
    text += QString("Time Step: %1\n").arg(time_step.time_);
    text += QString("Parts: %1\n\n").arg(time_step.parts_.size());

    // Mesh Info
    text += "Mesh Info:\n";
    text += QString("  Total Points: %1\n").arg(total_points);
    text += QString("  Total Faces: %1\n\n").arg(total_faces);

    // Point Fields
    text += QString("Point Fields (%1):\n").arg(total_point_fields);
    if (total_point_fields == 0)
    {
        text += "  (None)\n\n";
    }
    else
    {
        for (const auto& part : time_step.parts_)
        {
            for (const auto& field : part.point_fields_)
            {
                QString field_type = (field.type_ == Type::SCALAR)   ? "Scalar"
                                     : (field.type_ == Type::VECTOR) ? "Vector"
                                                                     : "Tensor";
                text += QString("  • %1 (%2, %3 components)\n")
                            .arg(QString::fromStdString(field.name_), field_type)
                            .arg(field.num_components_);
            }
        }
        text += "\n";
    }

    // Cell Fields
    text += QString("Cell Fields (%1):\n").arg(total_cell_fields);
    if (total_cell_fields == 0)
    {
        text += "  (None)\n";
    }
    else
    {
        for (const auto& part : time_step.parts_)
        {
            for (const auto& field : part.cell_fields_)
            {
                QString field_type = (field.type_ == Type::SCALAR)   ? "Scalar"
                                     : (field.type_ == Type::VECTOR) ? "Vector"
                                                                     : "Tensor";
                text += QString("  • %1 (%2, %3 components)\n")
                            .arg(QString::fromStdString(field.name_), field_type)
                            .arg(field.num_components_);
            }
        }
    }

    properties_panel_widget_->setInfoText(text);
}

void MainWindow::openDirectory()
{
    // TODO: open directory
}

void MainWindow::onFieldSelectionChanged(int index)
{
    // 隐藏所有场量选项
    render_options_widget_->hideFieldOptions();

    if (index == 0) // "无"选项
    {
        gl_widget_->getCaseData()->steps_[0].activateField("无");
        gl_widget_->setUseFieldColoring(false);
        gl_widget_->setRenderMode(Mode::BASIC);
        gl_widget_->setSeedSphereEditingEnabled(false);
        return;
    }

    QString field_name = render_options_widget_->currentField();

    Type field_type = gl_widget_->getCaseData()->steps_[0].activateField(field_name.toStdString());
    if (field_type == Type::SCALAR)
    {
        gl_widget_->setSeedSphereEditingEnabled(false);
        gl_widget_->setRenderMode(Mode::BASIC);
        gl_widget_->setUseFieldColoring(true);
        render_options_widget_->showScalarOptions();
    }
    else if (field_type == Type::VECTOR)
    {
        gl_widget_->setSeedSphereEditingEnabled(false);
        render_options_widget_->showVectorOptions();
        onVectorRenderModeChanged(render_options_widget_->vectorRenderModeIndex());
    }
}

void MainWindow::updateFieldList()
{
    // 保存当前选择
    QString current_selection = render_options_widget_->currentField();

    const CaseData* case_data = gl_widget_->getCaseData();
    if (!case_data || case_data->steps_.empty())
    {
        return;
    }

    const TimeStepData& time_step = case_data->steps_[0];

    // 收集所有标量字段名称（去重）和所有矢量字段名称
    QSet<QString> scalar_field_names;
    QSet<QString> vector_field_name_set;

    for (const auto& part : time_step.parts_)
    {
        // 添加点场
        for (const auto& field : part.point_fields_)
        {
            if (field.type_ == Type::SCALAR)
            {
                scalar_field_names.insert(QString::fromStdString(field.name_));
            }
            else if (field.type_ == Type::VECTOR)
            {
                vector_field_name_set.insert(QString::fromStdString(field.name_));
            }
        }

        // 添加单元场
        for (const auto& field : part.cell_fields_)
        {
            if (field.type_ == Type::SCALAR)
            {
                scalar_field_names.insert(QString::fromStdString(field.name_));
            }
            else if (field.type_ == Type::VECTOR)
            {
                vector_field_name_set.insert(QString::fromStdString(field.name_));
            }
        }
    }

    // 按字母顺序组织场量列表（先标量后向量）
    QList<QString> sorted_scalar_names = scalar_field_names.values();
    std::sort(sorted_scalar_names.begin(), sorted_scalar_names.end());

    QList<QString> sorted_vector_names = vector_field_name_set.values();
    std::sort(sorted_vector_names.begin(), sorted_vector_names.end());

    QStringList all_fields;
    all_fields << "无";
    for (const QString& name : sorted_scalar_names)
    {
        all_fields << name;
    }
    for (const QString& name : sorted_vector_names)
    {
        all_fields << name;
    }

    render_options_widget_->setFieldItems(all_fields, current_selection);

    // 更新流线选项中的矢量场列表
    QStringList vector_field_names = sorted_vector_names;
    streamline_options_widget_->setAvailableVectorFields(vector_field_names);
}

void MainWindow::onMeshRenderModeChanged(int index)
{
    MeshRenderMode mode = static_cast<MeshRenderMode>(index);
    gl_widget_->setMeshRenderMode(mode);
}

void MainWindow::onColorSchemeChanged(int index)
{
    ColorScheme scheme = static_cast<ColorScheme>(index);
    gl_widget_->setColorScheme(scheme);
}

void MainWindow::onVectorRenderModeChanged(int index)
{
    const CaseData* case_data = gl_widget_->getCaseData();
    if (!case_data || case_data->steps_.empty() || case_data->steps_[0].parts_.empty())
    {
        return;
    }

    const Field* active_field = case_data->steps_[0].parts_[0].active_field_;
    if (!active_field || active_field->type_ != Type::VECTOR)
    {
        return;
    }

    // 0: Arrow (几何箭头), 1: Magnitude (基础网格三种模式 + 幅值上色)
    if (index == 0)
    {
        gl_widget_->setUseFieldColoring(false);
        gl_widget_->setRenderMode(Mode::ARROW);
    }
    else
    {
        gl_widget_->setRenderMode(Mode::BASIC);
        gl_widget_->setUseFieldColoring(true);
    }
}
// gl_widget_->setVectorRenderMode(mode);
// Note: onVectorRenderModeChanged has been removed in favor of StreamlineOptionsWidget

void MainWindow::onFpsUpdated(float fps)
{
    fps_label_->setText(QString::asprintf("FPS: %.1f", fps));
}

void MainWindow::onLodLevelChanged(LODLevel level)
{
    QString levelText;
    switch (level)
    {
        case LODLevel::HIGH:
            levelText = "HIGH (100%)";
            break;
        case LODLevel::MEDIUM:
            levelText = "MEDIUM (75%)";
            break;
        case LODLevel::LOW:
            levelText = "LOW (55%)";
            break;
        default:
            levelText = "UNKNOWN";
            break;
    }
    lod_label_->setText(QString("LOD: %1").arg(levelText));
}

void MainWindow::onLodCheckBoxToggled(bool checked)
{
    gl_widget_->setLODEnabled(checked);
}

void MainWindow::onLodLevelComboChanged(int index)
{
    const LODLevel level = (index == 0) ? LODLevel::MEDIUM : LODLevel::LOW;
    gl_widget_->setInteractionLODLevel(level);
}

void MainWindow::onSeedSphereRadiusChanged(double value)
{
    gl_widget_->setSeedSphereRadius(static_cast<float>(value));
}

void MainWindow::onSeedSphereCountChanged(int value)
{
    gl_widget_->setStreamlineSeedCount(value);
}

void MainWindow::onSeedSphereOffsetChanged(const QVector3D& offset)
{
    gl_widget_->setSeedSphereOffset(offset);
}

void MainWindow::onSeedSphereCenterChanged(const QVector3D& center, bool valid)
{
    if (!valid)
    {
        streamline_options_widget_->setSeedSphereCenter(QVector3D(0, 0, 0));
        return;
    }

    streamline_options_widget_->setSeedSphereCenter(center);
}

void MainWindow::onStreamlineVectorFieldChanged(const QString& field_name)
{
    if (field_name == "无")
    {
        gl_widget_->setRenderMode(Mode::BASIC);
        return;
    }

    gl_widget_->getCaseData()->steps_[0].activateField(field_name.toStdString());
    gl_widget_->setSeedSphereEditingEnabled(true);
    // gl_widget_->setRenderMode(Mode::ARROW);
}

void MainWindow::onStreamlineRenderModeChanged(int mode)
{
    // mode: 0 = solid, 1 = point cloud
    // 对应Streamline的渲染模式
    // 这可以在未来用于切换流线的渲染模式
}

void MainWindow::onStreamlineLodEnabledChanged(bool checked)
{
    gl_widget_->setLODEnabled(checked);
}

void MainWindow::onStreamlineLodLevelChanged(LODLevel level)
{
    gl_widget_->setInteractionLODLevel(level);
}

void MainWindow::onGenerateStreamlinesRequested()
{
    // 获取当前选中的矢量场
    QString vector_field = streamline_options_widget_->getSelectedVectorField();
    if (vector_field.isEmpty())
    {
        qWarning() << "No vector field selected for streamline generation";
        return;
    }

    CaseData* case_data = gl_widget_->getCaseData();
    if (!case_data || case_data->steps_.empty())
    {
        qWarning() << "No case data available";
        return;
    }

    // 激活矢量字段
    Type field_type = case_data->steps_[0].activateField(vector_field.toStdString());
    if (field_type != Type::VECTOR)
    {
        qWarning() << "Selected field is not a vector field";
        return;
    }

    // 更新矢量缓冲区，确保GPU数据是最新的
    case_data->steps_[0].updateVectorBuffer();

    // 确保种子球编辑模式已启用
    gl_widget_->setSeedSphereEditingEnabled(true);

    // 切换到流线渲染模式
    gl_widget_->setRenderMode(Mode::STREAMLINE);

    // 生成流线
    gl_widget_->regenerateStreamlinesFromSeedSphere();
}
