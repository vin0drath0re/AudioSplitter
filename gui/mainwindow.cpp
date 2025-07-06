#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);
    relay.initialize();
    refreshDeviceList();

    connect(ui->clearTargetsButton, &QPushButton::clicked, this, &MainWindow::handleClearTargets);
    connect(ui->startStopButton, &QPushButton::clicked, this, &MainWindow::handleStartStopRelay);
}

MainWindow::~MainWindow() {
    relay.shutdown();
    delete ui;
}

void MainWindow::refreshDeviceList() {
  
    QLayoutItem* child;
    while ((child = ui->availableList->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    for (auto device : relay.getAvailableDevices()) {
        addDeviceToUIList(device, DeviceRole::Available);
    }

}

void MainWindow::addDeviceToUIList(IMMDevice* device, DeviceRole role) {
    auto item = new DeviceItemWidget(device, role);

    // Connect signals
    connect(item, &DeviceItemWidget::deviceRoleChanged, this, &MainWindow::handleDeviceRoleChange);
    connect(item, &DeviceItemWidget::volumeChanged, this, [this, item](IMMDevice* d, int v) {
        if (item->getRole() == DeviceRole::Source)
            relay.setGlobalVolume(v / 100.0f);
        else
            relay.setDeviceVolume(d, v / 100.0f);
    });



    // Add to the correct layout
    switch (role) {
        case DeviceRole::Available:
            ui->availableList->addWidget(item);
            break;

        case DeviceRole::Target:
            ui->targetList->addWidget(item);
            break;

        case DeviceRole::Source:
            // Clear existing source if any
            if (sourceItem) {
                removeFromLayout(ui->sourceLayout, sourceItem->getDevice());
                addDeviceToUIList(sourceItem->getDevice(), DeviceRole::Available);
                delete sourceItem;
            }

            sourceItem = item;
            ui->sourceLayout->addWidget(sourceItem);
            break;
    }
}

void MainWindow::removeFromLayout(QVBoxLayout* layout, IMMDevice* device) {
    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem* item = layout->itemAt(i);
        auto widget = qobject_cast<DeviceItemWidget*>(item->widget());
        if (widget && widget->getDevice() == device) {
            layout->removeWidget(widget);
            delete widget;
            break;
        }
    }
}

void MainWindow::removeDeviceWidget(IMMDevice* device) {
    removeFromLayout(ui->availableList, device);
    removeFromLayout(ui->targetList, device);
    removeFromLayout(ui->sourceLayout, device);
}


void MainWindow::handleDeviceRoleChange(IMMDevice* device, DeviceRole newRole) {
    // Remove the widget from all possible layouts
    removeDeviceWidget(device);

    // Update backend relay logic and UI
    switch (newRole) {
        case DeviceRole::Target:
            relay.addOutputDevice(device);
            addDeviceToUIList(device, DeviceRole::Target);
            break;

        case DeviceRole::Source:
            relay.setLoopbackDevice(device);
            addDeviceToUIList(device, DeviceRole::Source);
            break;

        case DeviceRole::Available:
            relay.removeOutputDevice(device); // Safe even if it's not in targets
            if (sourceItem && sourceItem->getDevice() == device) {
                relay.setLoopbackDevice(nullptr);
                delete sourceItem;
                sourceItem = nullptr;
            }
            addDeviceToUIList(device, DeviceRole::Available);
            break;
    }
}




void MainWindow::handleClearTargets() {
    relay.removeAllOutputDevices();

    // Remove all DeviceItemWidgets from the target list
    while (ui->targetList->count() > 0) {
        QLayoutItem* item = ui->targetList->takeAt(0);
        if (item->widget()) {
            delete item->widget();  // deletes the DeviceItemWidget
        }
        delete item;
    }

    refreshDeviceList();

}

void MainWindow::handleStartStopRelay() {
    if (ui->startStopButton->text() == "Start Relay") {
        relay.start();
        ui->startStopButton->setText("Stop Relay");
        ui->relayStatusLabel->setText("Running");
        ui->relayStatusLabel->setStyleSheet("color: green");
    } else {
        relay.stop();
        ui->startStopButton->setText("Start Relay");
        ui->relayStatusLabel->setText("Stopped");
        ui->relayStatusLabel->setStyleSheet("color: red");
    }
}
