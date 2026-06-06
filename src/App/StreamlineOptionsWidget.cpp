#include "StreamlineOptionsWidget.h"
#include "GLWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

StreamlineOptionsWidget::StreamlineOptionsWidget(GLWidget* gl_widget, QWidget* parent)
    : QWidget(parent), gl_widget_(gl_widget)
{
    setupUI();
}

void StreamlineOptionsWidget::setupUI()
{
    auto* main_layout = new QVBoxLayout(this);

    // Vector Field Selection Group
    auto* field_group = new QGroupBox("向量场选择");
    auto* field_layout = new QVBoxLayout();
    vector_field_combo_ = new QComboBox();
    vector_field_combo_->addItem("无");
    field_layout->addWidget(vector_field_combo_);
    field_group->setLayout(field_layout);
    main_layout->addWidget(field_group);

    // Streamline Render Mode Group
    auto* render_mode_group = new QGroupBox("流线渲染模式");
    auto* render_mode_layout = new QVBoxLayout();
    streamline_render_mode_combo_ = new QComboBox();
    streamline_render_mode_combo_->addItem("实体渲染");
    streamline_render_mode_combo_->addItem("点云渲染");
    render_mode_layout->addWidget(streamline_render_mode_combo_);
    render_mode_group->setLayout(render_mode_layout);
    main_layout->addWidget(render_mode_group);

    // LOD Optimization Group
    auto* lod_group = new QGroupBox("LOD优化");
    auto* lod_layout = new QVBoxLayout();

    streamline_lod_checkbox_ = new QCheckBox("启用LOD");
    streamline_lod_checkbox_->setChecked(false);
    lod_layout->addWidget(streamline_lod_checkbox_);

    auto* lod_level_layout = new QHBoxLayout();
    lod_level_layout->addWidget(new QLabel("交互LOD:"));
    streamline_lod_combo_ = new QComboBox();
    streamline_lod_combo_->addItem("自动: 近处中细节 / 远处低细节");
    streamline_lod_combo_->setCurrentIndex(0);
    streamline_lod_combo_->setEnabled(false);
    lod_level_layout->addWidget(streamline_lod_combo_);
    lod_layout->addLayout(lod_level_layout);

    lod_group->setLayout(lod_layout);
    main_layout->addWidget(lod_group);

    // Seed Sphere Controls Group
    seed_sphere_group_ = new QGroupBox("流线种子球");
    auto* seed_layout = new QVBoxLayout();

    seed_center_label_ = new QLabel("中心: (0.000, 0.000, 0.000)");
    seed_layout->addWidget(seed_center_label_);

    auto* radius_layout = new QHBoxLayout();
    radius_layout->addWidget(new QLabel("半径:"));
    seed_radius_spin_ = new QDoubleSpinBox();
    seed_radius_spin_->setDecimals(4);
    seed_radius_spin_->setRange(0.0001, 1e6);
    seed_radius_spin_->setSingleStep(0.01);
    seed_radius_spin_->setValue(gl_widget_->getSeedSphereRadius());
    radius_layout->addWidget(seed_radius_spin_);
    seed_layout->addLayout(radius_layout);

    auto* count_layout = new QHBoxLayout();
    count_layout->addWidget(new QLabel("种子数:"));
    seed_count_spin_ = new QSpinBox();
    seed_count_spin_->setRange(1, 20000);
    seed_count_spin_->setSingleStep(100);
    seed_count_spin_->setValue(gl_widget_->getStreamlineSeedCount());
    count_layout->addWidget(seed_count_spin_);
    seed_layout->addLayout(count_layout);

    auto* offset_x_layout = new QHBoxLayout();
    offset_x_layout->addWidget(new QLabel("偏移 X:"));
    seed_offset_x_spin_ = new QDoubleSpinBox();
    seed_offset_x_spin_->setDecimals(4);
    seed_offset_x_spin_->setRange(-1e6, 1e6);
    seed_offset_x_spin_->setSingleStep(0.01);
    offset_x_layout->addWidget(seed_offset_x_spin_);
    seed_layout->addLayout(offset_x_layout);

    auto* offset_y_layout = new QHBoxLayout();
    offset_y_layout->addWidget(new QLabel("偏移 Y:"));
    seed_offset_y_spin_ = new QDoubleSpinBox();
    seed_offset_y_spin_->setDecimals(4);
    seed_offset_y_spin_->setRange(-1e6, 1e6);
    seed_offset_y_spin_->setSingleStep(0.01);
    offset_y_layout->addWidget(seed_offset_y_spin_);
    seed_layout->addLayout(offset_y_layout);

    auto* offset_z_layout = new QHBoxLayout();
    offset_z_layout->addWidget(new QLabel("偏移 Z:"));
    seed_offset_z_spin_ = new QDoubleSpinBox();
    seed_offset_z_spin_->setDecimals(4);
    seed_offset_z_spin_->setRange(-1e6, 1e6);
    seed_offset_z_spin_->setSingleStep(0.01);
    offset_z_layout->addWidget(seed_offset_z_spin_);
    seed_layout->addLayout(offset_z_layout);

    auto* hint_label = new QLabel("提示: 按住左键拖动球体移动");
    seed_layout->addWidget(hint_label);

    generate_button_ = new QPushButton("生成流线");
    seed_layout->addWidget(generate_button_);

    seed_sphere_group_->setLayout(seed_layout);
    main_layout->addWidget(seed_sphere_group_);

    main_layout->addStretch();

    // Initialize offset values
    const QVector3D init_offset = gl_widget_->getSeedSphereOffset();
    seed_offset_x_spin_->setValue(init_offset.x());
    seed_offset_y_spin_->setValue(init_offset.y());
    seed_offset_z_spin_->setValue(init_offset.z());

    // Connect signals
    connect(vector_field_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &StreamlineOptionsWidget::onVectorFieldChanged);
    connect(streamline_render_mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &StreamlineOptionsWidget::onRenderModeChanged);
    connect(streamline_lod_checkbox_, &QCheckBox::toggled, this, [this](bool checked) {
        streamline_lod_combo_->setEnabled(checked);
        emit streamlineLodEnabledChanged(checked);
    });
    connect(seed_radius_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &StreamlineOptionsWidget::onSeedRadiusChanged);
    connect(seed_count_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &StreamlineOptionsWidget::onSeedCountChanged);
    connect(seed_offset_x_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &StreamlineOptionsWidget::onSeedOffsetXChanged);
    connect(seed_offset_y_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &StreamlineOptionsWidget::onSeedOffsetYChanged);
    connect(seed_offset_z_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &StreamlineOptionsWidget::onSeedOffsetZChanged);
    connect(generate_button_, &QPushButton::clicked, this,
            &StreamlineOptionsWidget::onGenerateButtonClicked);
}

void StreamlineOptionsWidget::setAvailableVectorFields(const QStringList& fields)
{
    updating_ = true;
    vector_field_combo_->clear();
    vector_field_combo_->addItem("无");
    vector_field_combo_->addItems(fields);
    updating_ = false;
}

void StreamlineOptionsWidget::setSeedSphereCenter(const QVector3D& center)
{
    seed_center_label_->setText(QString("中心: (%1, %2, %3)")
                                   .arg(center.x(), 0, 'f', 4)
                                   .arg(center.y(), 0, 'f', 4)
                                   .arg(center.z(), 0, 'f', 4));
}

void StreamlineOptionsWidget::setGenerationRunning(bool running)
{
    generate_button_->setEnabled(!running);
}

int StreamlineOptionsWidget::getStreamlineSeedCount() const
{
    return seed_count_spin_->value();
}

double StreamlineOptionsWidget::getSeedSphereRadius() const
{
    return seed_radius_spin_->value();
}

QVector3D StreamlineOptionsWidget::getSeedSphereOffset() const
{
    return QVector3D(seed_offset_x_spin_->value(), 
                     seed_offset_y_spin_->value(), 
                     seed_offset_z_spin_->value());
}

QString StreamlineOptionsWidget::getSelectedVectorField() const
{
    QString field = vector_field_combo_->currentText();
    return (field == "无") ? "" : field;
}

void StreamlineOptionsWidget::onVectorFieldChanged(int index)
{
    if (!updating_)
    {
        emit streamlineVectorFieldChanged(vector_field_combo_->currentText());
    }
}

void StreamlineOptionsWidget::onRenderModeChanged(int index)
{
    emit streamlineRenderModeChanged(index);
}

void StreamlineOptionsWidget::onSeedRadiusChanged(double value)
{
    emit seedSphereRadiusChanged(value);
}

void StreamlineOptionsWidget::onSeedCountChanged(int value)
{
    emit seedSphereCountChanged(value);
}

void StreamlineOptionsWidget::onSeedOffsetXChanged(double value)
{
    updateOffsetSignals();
}

void StreamlineOptionsWidget::onSeedOffsetYChanged(double value)
{
    updateOffsetSignals();
}

void StreamlineOptionsWidget::onSeedOffsetZChanged(double value)
{
    updateOffsetSignals();
}

void StreamlineOptionsWidget::updateOffsetSignals()
{
    emit seedSphereOffsetChanged(getSeedSphereOffset());
}

void StreamlineOptionsWidget::onGenerateButtonClicked()
{
    emit generateStreamlinesRequested();
}
