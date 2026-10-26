#pragma once

#include <QDialog>
#include <vector>

#include "domain/component.h"

class LabelDialog : public QDialog {
    Q_OBJECT
public:
    explicit LabelDialog(const std::vector<domain::Component>& components, QWidget* parent = nullptr);
};
