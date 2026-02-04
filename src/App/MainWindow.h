#pragma once

#include <QMainWindow>
#include <QMenu>
#include <QAction>
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow() = default;
    void test_openFile(const QString& filename);
private:

    void setupMenus();

    QAction* openFile_;
    QAction* openDir_;
    
private slots:
    void openFile();
    void openDirectory();
};