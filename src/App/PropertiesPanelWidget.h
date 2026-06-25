#pragma once

#include <QLabel>
#include <QString>
#include <QWidget>

class PropertiesPanelWidget : public QWidget
{
public:
    explicit PropertiesPanelWidget(QWidget* parent = nullptr);
    ~PropertiesPanelWidget() override = default;

    void setFilePathText(const QString& text);
    void setInfoText(const QString& text);

private:
    void setupUI();

    QLabel* file_path_label_ = nullptr;
    QLabel* info_label_ = nullptr;
};
