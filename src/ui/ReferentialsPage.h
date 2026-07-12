// ReferentialsPage — administration des référentiels (listes de valeurs
// normalisées) : ajout, modification, suppression, réordonnancement, fusion,
// import/export. Pour les référentiels liés à un champ de composant (type,
// fabricant, fournisseur, état), les renommages et fusions sont propagés aux
// composants concernés pour garder la base cohérente.
#pragma once
#include <QWidget>
#include <QString>

class QListWidget;
namespace chdesktop { class AppContext; }

class ReferentialsPage : public QWidget {
    Q_OBJECT
public:
    explicit ReferentialsPage(chdesktop::AppContext& ctx, QWidget* parent = nullptr);
protected:
    void showEvent(QShowEvent*) override;
private:
    void loadDefinitions();
    void selectReferential();
    void reloadValues();
    void addValue();
    void editValue();
    void deleteValue();
    void moveValue(int delta);
    void mergeValues();
    void importCsv();
    void exportCsv();

    QString currentKey() const;
    int currentBind() const;
    void seedIfEmpty(const QString& key, int bind);
    int  reassignComponentField(const QString& from, const QString& to, int bind);  // renvoie le nb de composants modifiés
    int  usageCount(const QString& value, int bind) const;

    chdesktop::AppContext& ctx_;
    QListWidget* referentials_ = nullptr;  // les listes disponibles
    QListWidget* values_ = nullptr;        // valeurs de la liste sélectionnée
};
