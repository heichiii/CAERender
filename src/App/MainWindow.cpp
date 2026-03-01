#include "MainWindow.h"
#include "TestTool/debug.h"
#include <QAction>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QCheckBox>
#include <QMenuBar>
#include <QSignalBlocker>
#include <QWidgetAction>
#include <QVBoxLayout>
#include <QLabel>
#include <QWidget>
#include "Loader/LoaderFactory.h"
#include "TestTool/Profiler.h"
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
    render_dock_ = new QDockWidget("Render Options", this);
    render_dock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, render_dock_);

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
    auto* propertiesAction = new QWidgetAction(view);
    auto* propertiesCheckBox = new QCheckBox("Properties", view);
    renderOptionsCheckBox->setChecked(true);
    propertiesCheckBox->setChecked(true);
    renderOptionsAction->setDefaultWidget(renderOptionsCheckBox);
    propertiesAction->setDefaultWidget(propertiesCheckBox);
    view->addAction(renderOptionsAction);
    view->addAction(propertiesAction);

    connect(renderOptionsCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        render_dock_->setVisible(checked);
    });

    connect(propertiesCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        properties_dock_->setVisible(checked);
    });

    connect(render_dock_, &QDockWidget::visibilityChanged, this, [renderOptionsCheckBox](bool visible) {
        QSignalBlocker blocker1(renderOptionsCheckBox);
        renderOptionsCheckBox->setChecked(visible);
    });

    connect(properties_dock_, &QDockWidget::visibilityChanged, this, [propertiesCheckBox](bool visible) {
        QSignalBlocker blocker2(propertiesCheckBox);
        propertiesCheckBox->setChecked(visible);
    });
}
void MainWindow::test_openFile()
{
    openFile();
}
void MainWindow::openFile()
{
    PROFILE_CODE
#if DEBUG_MODE
    QString filename = "E:/data/CAE/VTK/motorBike_500.vtk";
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
    int total_cells = 0;
    int total_point_fields = 0;
    int total_cell_fields = 0;
    
    for (const auto& part : time_step.parts_)
    {
        total_points += part.vertices_.size() / 3;  // 3个浮点数为一个点
        total_cells += part.faces_.size();
        total_point_fields += part.point_fields_.size();
        total_cell_fields += part.cell_fields_.size();
    }
    
    QString text;
    text += QString("Time Step: %1\n").arg(time_step.time_);
    text += QString("Parts: %1\n\n").arg(time_step.parts_.size());
    
    // Mesh Info
    text += "Mesh Info:\n";
    text += QString("  Total Points: %1\n").arg(total_points);
    text += QString("  Total Cells: %1\n\n").arg(total_cells);
    
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
                QString field_type = (field.type_ == Type::SCALAR) ? "Scalar" : 
                                     (field.type_ == Type::VECTOR) ? "Vector" : "Tensor";
                text += QString("  • %1 (%2, %3 components)\n")
                    .arg(QString::fromStdString(field.name_), field_type).arg(field.num_components_);
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
                QString field_type = (field.type_ == Type::SCALAR) ? "Scalar" : 
                                     (field.type_ == Type::VECTOR) ? "Vector" : "Tensor";
                text += QString("  • %1 (%2, %3 components)\n")
                    .arg(QString::fromStdString(field.name_), field_type).arg(field.num_components_);
            }
        }
    }
    
    info_label_->setText(text);
}

void MainWindow::openDirectory()
{
    // TODO: open directory
}
