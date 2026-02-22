#include "GLWidget.h"
#include "Loader/LoaderFactory.h"
void GLWidget::loadFile(const std::string& filename)
{
    auto loader = LoaderFactory::createLoader(filename);
    if (!loader)
    {
        qWarning() << "[GLWidget::loadFile] :  Unsupported file format:" << QString::fromStdString(filename);
        return;
    }

    case_data_.timeSteps_.clear();
    TimeStepData timeStep;
    timeStep.time_ = -1; // 单文件
    timeStep.parts_.push_back(loader->load());
    timeStep.generateGPUData();
    case_data_.timeSteps_.push_back(std::move(timeStep));

    update(); // 触发重绘
}