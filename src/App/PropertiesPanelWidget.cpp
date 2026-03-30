#include "PropertiesPanelWidget.h"

#include <QVBoxLayout>

PropertiesPanelWidget::PropertiesPanelWidget(QWidget* parent) : QWidget(parent)
{
    setupUI();
}

void PropertiesPanelWidget::setupUI()
{
    auto* layout = new QVBoxLayout(this);

    file_path_label_ = new QLabel("File: No file loaded", this);
    file_path_label_->setWordWrap(true);
    layout->addWidget(new QLabel("<b>File Path:</b>", this));
    layout->addWidget(file_path_label_);

    layout->addSpacing(10);

    info_label_ = new QLabel(this);
    info_label_->setWordWrap(true);
    info_label_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    layout->addWidget(new QLabel("<b>Mesh & Field Info:</b>", this));
    layout->addWidget(info_label_);

    layout->addStretch();
}

void PropertiesPanelWidget::setFilePathText(const QString& text)
{
    file_path_label_->setText(text);
}

void PropertiesPanelWidget::setInfoText(const QString& text)
{
    info_label_->setText(text);
}
