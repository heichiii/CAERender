#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
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
    QComboBox* colorSchemeCombo() const;
    QComboBox* vectorRenderModeCombo() const;

    QString currentField() const;
    int currentFieldIndex() const;
    int vectorRenderModeIndex() const;

    void setFieldItems(const QStringList& fields, const QString& current_selection);
    void showScalarOptions();
    void showVectorOptions();
    void hideFieldOptions();

private:
    void setupUI();

    QComboBox* mesh_render_mode_combo_ = nullptr;
    QCheckBox* lod_enable_checkbox_ = nullptr;
    QComboBox* field_combo_ = nullptr;

    QLabel* color_scheme_label_ = nullptr;
    QComboBox* color_scheme_combo_ = nullptr;
    QLabel* vector_render_mode_label_ = nullptr;
    QComboBox* vector_render_mode_combo_ = nullptr;
};
