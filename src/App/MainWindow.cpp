#include "MainWindow.h"
#include "Loader/LoaderFactory.h"
#include "TestTool/Profiler.h"
#include "TestTool/debug.h"
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>
#include <QWidgetAction>
#include <QStatusBar>
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("CAE Renderer");
    resize(1200, 800);
    setupUI();
    setupMenus();
}

void MainWindow::setupUI()
{
    gl_widget_ = new GLWidget(this);
    setCentralWidget(gl_widget_);

    // 创建 Render Options Dock (仅用于网格和场量)
    render_dock_ = new QDockWidget("Render Options", this);
    render_dock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    // 创建 render_dock_ 的内容面板
    auto* render_widget = new QWidget();
    auto* render_layout = new QVBoxLayout(render_widget);

    // 基础网格渲染模式
    auto* mesh_group = new QGroupBox("基础网格");
    auto* mesh_layout = new QVBoxLayout();
    mesh_render_mode_combo_ = new QComboBox();
    mesh_render_mode_combo_->addItem("实体渲染");
    mesh_render_mode_combo_->addItem("点云渲染");
    mesh_render_mode_combo_->addItem("线框渲染");
    mesh_layout->addWidget(mesh_render_mode_combo_);
    mesh_group->setLayout(mesh_layout);
    render_layout->addWidget(mesh_group);

    // LOD控制
    auto* lod_group = new QGroupBox("LOD优化");
    auto* lod_layout = new QVBoxLayout();
    lod_enable_checkbox_ = new QCheckBox("启用LOD");
    lod_enable_checkbox_->setChecked(true);
    lod_layout->addWidget(lod_enable_checkbox_);
    lod_group->setLayout(lod_layout);
    render_layout->addWidget(lod_group);

    // 场量选择
    auto* field_group = new QGroupBox("场量");
    auto* field_layout = new QVBoxLayout();
    field_combo_ = new QComboBox();
    field_combo_->addItem("无");
    field_layout->addWidget(field_combo_);
    field_group->setLayout(field_layout);
    render_layout->addWidget(field_group);

    // 场量选项（仅用于标量场）
    field_options_widget_ = new QWidget();
    auto* options_layout = new QVBoxLayout(field_options_widget_);
    options_layout->setContentsMargins(0, 0, 0, 0);

    // 配色方案（用于标量场）
    color_scheme_label_ = new QLabel("配色方案:");
    color_scheme_combo_ = new QComboBox();
    color_scheme_combo_->addItem("彩虹");
    color_scheme_combo_->addItem("热力图");
    color_scheme_combo_->addItem("冷暖");
    color_scheme_combo_->addItem("灰度");
    color_scheme_combo_->addItem("蓝白红");
    options_layout->addWidget(color_scheme_label_);
    options_layout->addWidget(color_scheme_combo_);

    // 向量场渲染方式（Arrow / Magnitude）
    vector_render_mode_label_ = new QLabel("向量渲染:");
    vector_render_mode_combo_ = new QComboBox();
    vector_render_mode_combo_->addItem("Arrow");
    vector_render_mode_combo_->addItem("Magnitude");
    options_layout->addWidget(vector_render_mode_label_);
    options_layout->addWidget(vector_render_mode_combo_);

    // 默认隐藏色彩方案
    color_scheme_label_->hide();
    color_scheme_combo_->hide();
    vector_render_mode_label_->hide();
    vector_render_mode_combo_->hide();

    render_layout->addWidget(field_options_widget_);
    render_layout->addStretch();

    render_widget->setLayout(render_layout);
    render_dock_->setWidget(render_widget);
    addDockWidget(Qt::LeftDockWidgetArea, render_dock_);

    // 连接信号
    connect(field_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onFieldSelectionChanged);

    // 连接基础网格渲染模式
    connect(mesh_render_mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onMeshRenderModeChanged);

    // 连接LOD复选框
    connect(lod_enable_checkbox_, &QCheckBox::toggled, this, &MainWindow::onLodCheckBoxToggled);

    // 连接配色方案
    connect(color_scheme_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onColorSchemeChanged);

        connect(vector_render_mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onVectorRenderModeChanged);

    // 创建 Streamline Options Dock
    streamline_dock_ = new QDockWidget("Streamline Options", this);
    streamline_dock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    
    streamline_options_widget_ = new StreamlineOptionsWidget(gl_widget_, this);
    streamline_dock_->setWidget(streamline_options_widget_);
    addDockWidget(Qt::LeftDockWidgetArea, streamline_dock_);

    // 连接流线选项信号
    connect(streamline_options_widget_, &StreamlineOptionsWidget::streamlineVectorFieldChanged, this,
            &MainWindow::onStreamlineVectorFieldChanged);
    connect(streamline_options_widget_, &StreamlineOptionsWidget::streamlineRenderModeChanged, this,
            &MainWindow::onStreamlineRenderModeChanged);
    connect(streamline_options_widget_, &StreamlineOptionsWidget::streamlineLodLevelChanged, this,
            &MainWindow::onStreamlineLodLevelChanged);
    connect(streamline_options_widget_, &StreamlineOptionsWidget::seedSphereRadiusChanged, this,
            &MainWindow::onSeedSphereRadiusChanged);
    connect(streamline_options_widget_, &StreamlineOptionsWidget::seedSphereCountChanged, this,
            &MainWindow::onSeedSphereCountChanged);
    connect(streamline_options_widget_, &StreamlineOptionsWidget::seedSphereOffsetChanged, this,
            &MainWindow::onSeedSphereOffsetChanged);
    connect(streamline_options_widget_, &StreamlineOptionsWidget::generateStreamlinesRequested, this,
            &MainWindow::onGenerateStreamlinesRequested);

    // 创建 Properties Dock
    properties_dock_ = new QDockWidget("Properties", this);
    properties_dock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    // 创建 properties_dock_ 的内容面板
    auto* properties_widget = new QWidget();
    auto* layout = new QVBoxLayout(properties_widget);

    file_path_label_ = new QLabel("File: No file loaded");
    file_path_label_->setWordWrap(true);
    layout->addWidget(new QLabel("<b>File Path:</b>"));
    layout->addWidget(file_path_label_);

    layout->addSpacing(10);

    info_label_ = new QLabel();
    info_label_->setWordWrap(true);
    info_label_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    layout->addWidget(new QLabel("<b>Mesh & Field Info:</b>"));
    layout->addWidget(info_label_);

    layout->addStretch();
    properties_widget->setLayout(layout);
    properties_dock_->setWidget(properties_widget);

    addDockWidget(Qt::RightDockWidgetArea, properties_dock_);
    
    // 创建 Status Bar 显示帧率和LOD级别
    fps_label_ = new QLabel("FPS: 0.0", this);
    fps_label_->setAlignment(Qt::AlignLeft);
    fps_label_->setMinimumWidth(100);
    statusBar()->addWidget(fps_label_);
    
    lod_label_ = new QLabel("LOD: HIGH", this);
    lod_label_->setAlignment(Qt::AlignLeft);
    lod_label_->setMinimumWidth(120);
    statusBar()->addWidget(lod_label_);
    
    // 连接GLWidget的信号
    connect(gl_widget_, &GLWidget::fpsUpdated, this, &MainWindow::onFpsUpdated);
    connect(gl_widget_, &GLWidget::lodLevelChanged, this, &MainWindow::onLodLevelChanged);
    connect(gl_widget_, &GLWidget::seedSphereCenterChanged, this,
            &MainWindow::onSeedSphereCenterChanged);
}

void MainWindow::setupMenus()
{
    QMenu* file = menuBar()->addMenu("&File");
    openfile_ = file->addAction("Open &File...");
    opendir_ = file->addAction("Open &Directory...");
    connect(openfile_, &QAction::triggered, this, &MainWindow::openFile);
    connect(opendir_, &QAction::triggered, this, &MainWindow::openDirectory);

    QMenu* view = menuBar()->addMenu("&View");
    auto* renderOptionsAction = new QWidgetAction(view);
    auto* renderOptionsCheckBox = new QCheckBox("Render Options", view);
    auto* streamlineOptionsAction = new QWidgetAction(view);
    auto* streamlineOptionsCheckBox = new QCheckBox("Streamline Options", view);
    auto* propertiesAction = new QWidgetAction(view);
    auto* propertiesCheckBox = new QCheckBox("Properties", view);
    renderOptionsCheckBox->setChecked(true);
    streamlineOptionsCheckBox->setChecked(true);
    propertiesCheckBox->setChecked(true);
    renderOptionsAction->setDefaultWidget(renderOptionsCheckBox);
    streamlineOptionsAction->setDefaultWidget(streamlineOptionsCheckBox);
    propertiesAction->setDefaultWidget(propertiesCheckBox);
    view->addAction(renderOptionsAction);
    view->addAction(streamlineOptionsAction);
    view->addAction(propertiesAction);

    connect(renderOptionsCheckBox, &QCheckBox::toggled, this,
            [this](bool checked) { render_dock_->setVisible(checked); });

    connect(streamlineOptionsCheckBox, &QCheckBox::toggled, this,
            [this](bool checked) { streamline_dock_->setVisible(checked); });

    connect(propertiesCheckBox, &QCheckBox::toggled, this,
            [this](bool checked) { properties_dock_->setVisible(checked); });

    connect(render_dock_, &QDockWidget::visibilityChanged, this,
            [renderOptionsCheckBox](bool visible)
            {
                QSignalBlocker blocker1(renderOptionsCheckBox);
                renderOptionsCheckBox->setChecked(visible);
            });

    connect(streamline_dock_, &QDockWidget::visibilityChanged, this,
            [streamlineOptionsCheckBox](bool visible)
            {
                QSignalBlocker blocker2_streamline(streamlineOptionsCheckBox);
                streamlineOptionsCheckBox->setChecked(visible);
            });

    connect(properties_dock_, &QDockWidget::visibilityChanged, this,
            [propertiesCheckBox](bool visible)
            {
                QSignalBlocker blocker2(propertiesCheckBox);
                propertiesCheckBox->setChecked(visible);
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
    PROFILE_CODE
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
    file_path_label_->setText(QString::fromStdString(current_file_path_));

    const CaseData* case_data = gl_widget_->getCaseData();
    if (!case_data || case_data->steps_.empty())
    {
        info_label_->setText("No data loaded.");
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

    info_label_->setText(text);
}

void MainWindow::openDirectory()
{
    // TODO: open directory
}

void MainWindow::onFieldSelectionChanged(int index)
{
    // 隐藏所有场量选项
    color_scheme_label_->hide();
    color_scheme_combo_->hide();
    vector_render_mode_label_->hide();
    vector_render_mode_combo_->hide();

    if (index == 0) // "无"选项
    {
        gl_widget_->getCaseData()->steps_[0].activateField("无");
        gl_widget_->setUseFieldColoring(false);
        gl_widget_->setRenderMode(Mode::BASIC);
        gl_widget_->setSeedSphereEditingEnabled(false);
        return;
    }

    QString field_name = field_combo_->currentText();

    Type field_type = gl_widget_->getCaseData()->steps_[0].activateField(field_name.toStdString());
    if (field_type == Type::SCALAR)
    {
        gl_widget_->setSeedSphereEditingEnabled(false);
        gl_widget_->setRenderMode(Mode::BASIC);
        gl_widget_->setUseFieldColoring(true);
        color_scheme_label_->show();
        color_scheme_combo_->show();
    }
    else if (field_type == Type::VECTOR)
    {
        gl_widget_->setSeedSphereEditingEnabled(false);
        color_scheme_label_->show();
        color_scheme_combo_->show();
        vector_render_mode_label_->show();
        vector_render_mode_combo_->show();
        onVectorRenderModeChanged(vector_render_mode_combo_->currentIndex());
    }
}

void MainWindow::updateFieldList()
{
    // 保存当前选择
    QString current_selection = field_combo_->currentText();

    // 清空并重新填充场量列表
    field_combo_->clear();
    field_combo_->addItem("无");

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

    // 按字母顺序排序并添加标量字段到下拉框
    QList<QString> sorted_scalar_names = scalar_field_names.values();
    std::sort(sorted_scalar_names.begin(), sorted_scalar_names.end());

    for (const QString& name : sorted_scalar_names)
    {
        field_combo_->addItem(name);
    }

    QList<QString> sorted_vector_names = vector_field_name_set.values();
    std::sort(sorted_vector_names.begin(), sorted_vector_names.end());
    for (const QString& name : sorted_vector_names)
    {
        field_combo_->addItem(name);
    }

    // 更新流线选项中的矢量场列表
    QStringList vector_field_names = sorted_vector_names;
    streamline_options_widget_->setAvailableVectorFields(vector_field_names);

    // 尝试恢复之前的选择
    int index = field_combo_->findText(current_selection);
    if (index >= 0)
    {
        field_combo_->setCurrentIndex(index);
    }
    else
    {
        field_combo_->setCurrentIndex(0); // 默认选择"无"
    }
}

void MainWindow::updateFieldOptions()
{
    // 当场量列表更新时，更新场量选项
    onFieldSelectionChanged(field_combo_->currentIndex());
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
            levelText = "MEDIUM (50%)";
            break;
        case LODLevel::LOW:
            levelText = "LOW (25%)";
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
    gl_widget_->setRenderMode(Mode::ARROW);
}

void MainWindow::onStreamlineRenderModeChanged(int mode)
{
    // mode: 0 = solid, 1 = point cloud
    // 对应Streamline的渲染模式
    // 这可以在未来用于切换流线的渲染模式
}

void MainWindow::onStreamlineLodLevelChanged(LODLevel level)
{
    // 流线LOD级别变化处理
    // TODO: 实现流线的LOD优化
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
