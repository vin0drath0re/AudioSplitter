#include "DeviceItemWidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "../../core/relay.h"

DeviceItemWidget::DeviceItemWidget(IMMDevice* dev, DeviceRole role, QWidget* parent)
    : QWidget(parent), device(dev), role(role) {

    nameLabel = new QLabel(QString::fromStdWString(AudioRelay::getDeviceName(device)), this);
    volumeSlider = new QSlider(Qt::Horizontal, this);
    addButton = new QPushButton("Add", this);
    setSourceButton = new QPushButton("Set Source", this);
    removeButton = new QPushButton("✕", this);

    volumeSlider->setRange(0, 100);
    volumeSlider->setValue(100); // Initial full volume

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(nameLabel);
    layout->addWidget(volumeSlider);
    layout->addWidget(addButton);
    layout->addWidget(setSourceButton);
    layout->addWidget(removeButton);
    layout->setContentsMargins(2, 2, 2, 2);
    volumeSlider->setFixedWidth(100);
    addButton->setFixedWidth(80);
    setSourceButton->setFixedWidth(100);
    removeButton->setFixedWidth(30);
    addButton->setToolTip("Add to output");
    setSourceButton->setToolTip("Set as source");
    removeButton->setToolTip("Remove and return to available");
    volumeSlider->setToolTip("Adjust volume");


    setLayout(layout);

    setRole(role);


    connect(addButton, &QPushButton::clicked, this, [this]() {
        emit deviceRoleChanged(device, DeviceRole::Target);
    }); 

    connect(setSourceButton, &QPushButton::clicked, this, [this]() {
        emit deviceRoleChanged(device, DeviceRole::Source); 
    });

    connect(removeButton, &QPushButton::clicked, this, [this]() {
        emit deviceRoleChanged(device, DeviceRole::Available);
    });

    connect(volumeSlider, &QSlider::valueChanged, this, [this](int value) {
        emit volumeChanged(device, value);
    });
}

void DeviceItemWidget::setRole(DeviceRole newRole) {
    role = newRole;

    // Reset visibility
    addButton->setVisible(false);
    setSourceButton->setVisible(false);
    removeButton->setVisible(false);
    volumeSlider->setVisible(false);

    switch (role) {
        case DeviceRole::Available:
            addButton->setVisible(true);
            setSourceButton->setVisible(true);
            break;
        case DeviceRole::Target:
            volumeSlider->setVisible(true);
            removeButton->setVisible(true);
            break;
        case DeviceRole::Source:
            volumeSlider->setVisible(true);
            removeButton->setVisible(true);
            break;
    }
}

void DeviceItemWidget::setDeviceName(const QString& name) {
    nameLabel->setText(name);
}

void DeviceItemWidget::setVolume(int volume) {
    volumeSlider->setValue(volume);
}

int DeviceItemWidget::getVolume() const {
    return volumeSlider->value();
}

DeviceRole DeviceItemWidget::getRole() const {
    return role;
}

IMMDevice* DeviceItemWidget::getDevice() const {
    return device;
}