#include "RenderOptionsWidget.h"

#include <QGroupBox>
#include <QVBoxLayout>

RenderOptionsWidget::RenderOptionsWidget(QWidget* parent) : QWidget(parent)
{
    setupUI();
}

void RenderOptionsWidget::setupUI()
{

    auto* render_layout = new QVBoxLayout(this);

    auto* mesh_group = new QGroupBox("基础网格", this);
    auto* mesh_layout = new QVBoxLayout(mesh_group);
    mesh_render_mode_combo_ = new QComboBox(mesh_group);
    mesh_render_mode_combo_->addItem("实体渲染");
    mesh_render_mode_combo_->addItem("点云渲染");
    mesh_render_mode_combo_->addItem("线框渲染");
    mesh_layout->addWidget(mesh_render_mode_combo_);
    render_layout->addWidget(mesh_group);

    auto* lod_group = new QGroupBox("LOD优化", this);
    auto* lod_layout = new QVBoxLayout(lod_group);
    lod_enable_checkbox_ = new QCheckBox("启用LOD", lod_group);
    lod_enable_checkbox_->setChecked(true);
    lod_layout->addWidget(lod_enable_checkbox_);
    render_layout->addWidget(lod_group);

    auto* field_group = new QGroupBox("场量", this);
    auto* field_layout = new QVBoxLayout(field_group);
    field_combo_ = new QComboBox(field_group);
    field_combo_->addItem("无");
    field_layout->addWidget(field_combo_);
    render_layout->addWidget(field_group);

    
    auto* field_options_widget = new QWidget(this);
    auto* options_layout = new QVBoxLayout(field_options_widget);
    options_layout->setContentsMargins(0, 0, 0, 0);

    color_scheme_label_ = new QLabel("配色方案:", field_options_widget);
    color_scheme_combo_ = new QComboBox(field_options_widget);
    color_scheme_combo_->addItem("彩虹");
    color_scheme_combo_->addItem("热力图");
    color_scheme_combo_->addItem("冷暖");
    color_scheme_combo_->addItem("灰度");
    color_scheme_combo_->addItem("蓝白红");
    options_layout->addWidget(color_scheme_label_);
    options_layout->addWidget(color_scheme_combo_);

    vector_render_mode_label_ = new QLabel("向量渲染:", field_options_widget);
    vector_render_mode_combo_ = new QComboBox(field_options_widget);
    vector_render_mode_combo_->addItem("Arrow");
    vector_render_mode_combo_->addItem("Magnitude");
    options_layout->addWidget(vector_render_mode_label_);
    options_layout->addWidget(vector_render_mode_combo_);

    hideFieldOptions();

    render_layout->addWidget(field_options_widget);
    render_layout->addStretch();
}

QComboBox* RenderOptionsWidget::fieldCombo() const
{
    return field_combo_;
}

QComboBox* RenderOptionsWidget::meshRenderModeCombo() const
{
    return mesh_render_mode_combo_;
}

QCheckBox* RenderOptionsWidget::lodCheckBox() const
{
    return lod_enable_checkbox_;
}

QComboBox* RenderOptionsWidget::colorSchemeCombo() const
{
    return color_scheme_combo_;
}

QComboBox* RenderOptionsWidget::vectorRenderModeCombo() const
{
    return vector_render_mode_combo_;
}

QString RenderOptionsWidget::currentField() const
{
    return field_combo_->currentText();
}

int RenderOptionsWidget::currentFieldIndex() const
{
    return field_combo_->currentIndex();
}

int RenderOptionsWidget::vectorRenderModeIndex() const
{
    return vector_render_mode_combo_->currentIndex();
}

void RenderOptionsWidget::setFieldItems(const QStringList& fields, const QString& current_selection)
{
    field_combo_->clear();
    field_combo_->addItems(fields);

    int index = field_combo_->findText(current_selection);
    if (index >= 0)
    {
        field_combo_->setCurrentIndex(index);
    }
    else
    {
        field_combo_->setCurrentIndex(0);
    }
}

void RenderOptionsWidget::showScalarOptions()
{
    color_scheme_label_->show();
    color_scheme_combo_->show();
    vector_render_mode_label_->hide();
    vector_render_mode_combo_->hide();
}

void RenderOptionsWidget::showVectorOptions()
{
    color_scheme_label_->show();
    color_scheme_combo_->show();
    vector_render_mode_label_->show();
    vector_render_mode_combo_->show();
}

void RenderOptionsWidget::hideFieldOptions()
{
    color_scheme_label_->hide();
    color_scheme_combo_->hide();
    vector_render_mode_label_->hide();
    vector_render_mode_combo_->hide();
}
