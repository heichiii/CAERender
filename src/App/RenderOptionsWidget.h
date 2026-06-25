#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QStringList>
#include <QWidget>

class RenderOptionsWidget : public QWidget
{
public:
    explicit RenderOptionsWidget(QWidget* parent = nullptr);
    ~RenderOptionsWidget() override = default;

    QComboBox* fieldCombo() const;
    QComboBox* meshRenderModeCombo() const;
    QCheckBox* lodCheckBox() const;
    QComboBox* lodLevelCombo() const;
    QComboBox* colorSchemeCombo() const;
    QComboBox* vectorRenderModeCombo() const;

    QSlider* timeStepSlider() const;
    QSpinBox* timeStepSpinBox() const;

    QString currentField() const;
    int currentFieldIndex() const;
    int vectorRenderModeIndex() const;

    void setFieldItems(const QStringList& fields, const QString& current_selection);
    void setTimeStepRange(int min_step, int max_step);
    void setCurrentTimeStep(int step);
    void setTimeStepVisible(bool visible);
    
    void showScalarOptions();
    void showVectorOptions();
    void hideFieldOptions();

private:
    void setupUI();

    QComboBox* mesh_render_mode_combo_ = nullptr;
    QCheckBox* lod_enable_checkbox_ = nullptr;
    QComboBox* lod_level_combo_ = nullptr;
    QComboBox* field_combo_ = nullptr;


    QGroupBox* time_step_group_ = nullptr;
    class QSlider* time_step_slider_ = nullptr;
    class QSpinBox* time_step_spinbox_ = nullptr;
    QLabel* color_scheme_label_ = nullptr;
    QComboBox* color_scheme_combo_ = nullptr;
    QLabel* vector_render_mode_label_ = nullptr;
    QComboBox* vector_render_mode_combo_ = nullptr;
};
