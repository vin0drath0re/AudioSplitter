#ifndef DEVICEITEMWIDGET_H
#define DEVICEITEMWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <mmdeviceapi.h>

enum class DeviceRole {
    Available,
    Target,
    Source
};

class DeviceItemWidget : public QWidget {
    Q_OBJECT

public:
    DeviceItemWidget(IMMDevice* dev, DeviceRole role, QWidget* parent = nullptr);

    void setRole(DeviceRole newRole);
    DeviceRole getRole() const;
    IMMDevice* getDevice() const;

    void setDeviceName(const QString& name);
    void setVolume(int volume);
    int getVolume() const;

signals:
    void deviceRoleChanged(IMMDevice* device, DeviceRole newRole);
    void volumeChanged(IMMDevice* device, int volume);

private:
    IMMDevice* device;
    DeviceRole role;

    QLabel* nameLabel;
    QSlider* volumeSlider;
    QPushButton* addButton;
    QPushButton* setSourceButton;
    QPushButton* removeButton;
};


#endif // DEVICEITEMWIDGET_H
