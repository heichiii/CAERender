#pragma once

#include "Render/Renderer.h"
#include <QWidget>
#include <QGroupBox>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QVector3D>

class GLWidget;

class StreamlineOptionsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit StreamlineOptionsWidget(GLWidget* gl_widget, QWidget* parent = nullptr);
    ~StreamlineOptionsWidget() override = default;

    // Getter methods
    int getStreamlineSeedCount() const;
    double getSeedSphereRadius() const;
    QVector3D getSeedSphereOffset() const;
    QString getSelectedVectorField() const;

    // Setter methods
    void setSeedSphereCenter(const QVector3D& center);
    void setAvailableVectorFields(const QStringList& fields);
    void setGenerationRunning(bool running);

signals:
    void streamlineVectorFieldChanged(const QString& field_name);
    void streamlineRenderModeChanged(int mode);  // 0: solid, 1: point cloud
    void streamlineLodEnabledChanged(bool enabled);
    void seedSphereRadiusChanged(double radius);
    void seedSphereCountChanged(int count);
    void seedSphereOffsetChanged(const QVector3D& offset);
    void generateStreamlinesRequested();

private slots:
    void onVectorFieldChanged(int index);
    void onRenderModeChanged(int index);
    void onSeedRadiusChanged(double value);
    void onSeedCountChanged(int value);
    void onSeedOffsetXChanged(double value);
    void onSeedOffsetYChanged(double value);
    void onSeedOffsetZChanged(double value);
    void onGenerateButtonClicked();

private:
    void setupUI();
    void updateOffsetSignals();

    GLWidget* gl_widget_;

    // Vector Field Selection
    QComboBox* vector_field_combo_;

    // Streamline Render Mode
    QComboBox* streamline_render_mode_combo_;

    // LOD Controls
    QCheckBox* streamline_lod_checkbox_;
    QComboBox* streamline_lod_combo_;

    // Seed Sphere Controls
    QGroupBox* seed_sphere_group_;
    QLabel* seed_center_label_;
    QDoubleSpinBox* seed_radius_spin_;
    QSpinBox* seed_count_spin_;
    QDoubleSpinBox* seed_offset_x_spin_;
    QDoubleSpinBox* seed_offset_y_spin_;
    QDoubleSpinBox* seed_offset_z_spin_;
    QPushButton* generate_button_;

    // Track if we're updating to avoid recursive signals
    bool updating_ = false;
};
