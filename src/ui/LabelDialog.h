#pragma once

#include <QDialog>
#include <vector>

#include "domain/component.h"

class LabelDialog : public QDialog {
    Q_OBJECT
public:
    explicit LabelDialog(const std::vector<domain::Component>& components, QWidget* parent = nullptr);

private:
    // Copie possedee par le dialogue : l'apercu et les boutons en gardent des pointeurs.
    std::vector<domain::Component> components_;
};
