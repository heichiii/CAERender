#pragma once
#include <QDebug> // 确保 qInfo 可用
#include <QElapsedTimer>
#include <QFileInfo>

// 辅助类：自动计时并在析构时输出
class Profiler
{
public:
    // 用于函数级别的计时（使用函数名）
    explicit Profiler(const char* func, const char* file, int line)
        : m_label(func), m_file(file), m_line(line), m_isCustomLabel(false)
    {
        m_timer.start();
    }

    // 用于代码块级别的计时（使用自定义标签）
    Profiler(const char* label, const char* file, int line, bool isCustom)
        : m_label(label), m_file(file), m_line(line), m_isCustomLabel(isCustom)
    {
        m_timer.start();
    }

    ~Profiler()
    {
        qint64 elapsed = m_timer.nsecsElapsed(); // 纳秒精度
        double ms = elapsed / 1000000.0;       // 转为毫秒
        QString fileName = QFileInfo(m_file).fileName();
        
        if (m_isCustomLabel) {
            // 代码块标签格式
            qInfo().noquote()
                << QString::asprintf("[%s:%d] %s took %.3f ms",
                                    fileName.toLocal8Bit().constData(),
                                    m_line,
                                    m_label,
                                    ms);
        } else {
            // 函数名格式（保持原样）
            qInfo().noquote()
                << QString::asprintf("[%s:%d] %s took %.3f ms",
                                    fileName.toLocal8Bit().constData(),
                                    m_line,
                                    m_label,
                                    ms);
        }
    }

private:
    QElapsedTimer m_timer;
    const char* m_label;
    const char* m_file;
    int m_line;
    bool m_isCustomLabel;
};

// 宏定义：自动捕获函数名、文件名、行号（用于整个函数）
#define PROFILE_CODE Profiler _profiler_##__LINE__(__FUNCTION__, __FILE__, __LINE__);

// 宏定义：自动捕获代码块标签、文件名、行号（用于代码块）
#define PROFILE_SCOPE(label) Profiler _profiler_##__LINE__(label, __FILE__, __LINE__, true);