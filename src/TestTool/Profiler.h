#pragma once
#include <QDebug> // 确保 qInfo 可用
#include <QElapsedTimer>
#include <QFileInfo>

// 辅助类：自动计时并在析构时输出
class Profiler
{
public:
    explicit Profiler(const char* func, const char* file, int line)
        : m_func(func), m_file(file), m_line(line)
    {
        m_timer.start();
    }

    ~Profiler()
    {
        qint64 elapsed = m_timer.nsecsElapsed(); // 纳秒精度
        double ms = elapsed / 1000000.0;       // 转为毫秒
        QString fileName = QFileInfo(m_file).fileName();
        qInfo().noquote()
            << QString::asprintf("[%s:%d] %s took %.3f ms",
                                fileName.toLocal8Bit().constData(),
                                m_line,
                                m_func,
                                ms);
    }

private:
    QElapsedTimer m_timer;
    const char* m_func;
    const char* m_file;
    int m_line;
};

// 宏定义：自动捕获函数名、文件名、行号
#define PROFILE_CODE Profiler _profiler_##__LINE__(__FUNCTION__, __FILE__, __LINE__);