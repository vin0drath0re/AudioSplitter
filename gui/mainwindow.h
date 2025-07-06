#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVBoxLayout>
#include <mmdeviceapi.h>
#include "../core/relay.h"
#include "components/DeviceItemWidget.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void handleClearTargets();
    void handleStartStopRelay();
    void handleDeviceRoleChange(IMMDevice* device, DeviceRole newRole);

private:
    Ui::MainWindow *ui;
    AudioRelay relay;
    DeviceItemWidget* sourceItem = nullptr;

    void refreshDeviceList();
    void addDeviceToUIList(IMMDevice* device, DeviceRole role);
    void removeFromLayout(QVBoxLayout* layout, IMMDevice* device);
    void removeDeviceWidget(IMMDevice* device);
};

#endif // MAINWINDOW_H
