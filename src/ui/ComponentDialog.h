// ComponentDialog — création/édition d'une fiche composant complète, organisée
// en onglets (Général, Caractéristiques, Achat & stock, Documents, Notes).
// Les documents et mouvements de stock ne sont disponibles qu'une fois la
// fiche enregistrée (ils référencent son id).
#pragma once
#include <QDialog>
#include "component.h"

class QLineEdit;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QPlainTextEdit;
class QTableWidget;
class QLabel;
namespace chdesktop { class AppContext; }

class ComponentDialog : public QDialog {
    Q_OBJECT
public:
    ComponentDialog(chdesktop::AppContext& ctx, domain::Id componentId, QWidget* parent = nullptr);
    domain::Id savedId() const { return current_.id; }

private:
    void buildUi();
    void loadToForm();
    bool saveFromForm();          // valide + persiste ; false si invalide
    void refreshChildTabs();      // documents + stock (selon existence de l'id)
    void addOrEditDocument(domain::Id docId);
    void deleteDocument();
    void doStockMovement();

    chdesktop::AppContext& ctx_;
    domain::Component current_;

    QComboBox*      kind_ = nullptr;
    QLineEdit*      name_ = nullptr;
    QLineEdit*      reference_ = nullptr;
    QLineEdit*      manufacturer_ = nullptr;
    QComboBox*      category_ = nullptr;
    QLineEdit*      subcategory_ = nullptr;
    QComboBox*      type_ = nullptr;   // référentiel « Types de composants » (éditable)
    QComboBox*      status_ = nullptr;
    QLineEdit*      state_ = nullptr;
    QLineEdit*      origin_ = nullptr;
    QComboBox*      location_ = nullptr;
    QPlainTextEdit* description_ = nullptr;

    QLineEdit* voltage_ = nullptr;
    QLineEdit* current_field_ = nullptr;
    QLineEdit* interfaceType_ = nullptr;
    QLineEdit* protocols_ = nullptr;
    QLineEdit* i2cAddress_ = nullptr;
    QLineEdit* frequency_ = nullptr;
    QSpinBox*  pinCount_ = nullptr;
    QLineEdit* compatibility_ = nullptr;

    QDoubleSpinBox* price_ = nullptr;
    QLineEdit* supplier_ = nullptr;
    QLineEdit* purchaseDate_ = nullptr;
    QLineEdit* receptionDate_ = nullptr;
    QLineEdit* warranty_ = nullptr;
    QSpinBox*  quantity_ = nullptr;
    QSpinBox*  minStock_ = nullptr;
    QSpinBox*  idealStock_ = nullptr;

    QPlainTextEdit* notes_ = nullptr;

    QTableWidget* documents_ = nullptr;
    QTableWidget* history_ = nullptr;
    QLabel* stockLabel_ = nullptr;
    QLabel* childHint_ = nullptr;
};
