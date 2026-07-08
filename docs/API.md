# Référence API — ComponentHub

Toutes les routes renvoient et acceptent du JSON (`application/json`), sauf
mention contraire. Les identifiants (`id`) sont des entiers ; `0` (ou champ
absent) signifie « pas d'identifiant / aucun filtre » selon le contexte.

Cette référence couvre les routes propres à ComponentHub. Les routes
héritées d'ESP32-Foundation (système, logs, fichiers, OTA) sont listées en
fin de document.

## Inventaire — composants

Module : `src/modules/inventory/`.

### `GET /api/inventory/components`

Liste filtrée des composants. Paramètres de requête, tous optionnels et
combinables :

| Paramètre | Valeurs | Effet |
|---|---|---|
| `q` | texte libre | Recherche dans nom, référence, fabricant, catégorie, fournisseur, notes. |
| `kind` | `component`\|`module`\|`tool`\|`consumable` | Filtre par type. |
| `category` | texte | Filtre exact sur la catégorie. |
| `status` | `to_test`\|`validating`\|`validated`\|`defective`\|`archived` | Filtre exact sur le statut de validation. |
| `locationId` | entier | Filtre par emplacement. |
| `lowStock` | `1` | Ne garde que les composants sous leur seuil minimum. |

Réponse : tableau d'objets `Component` (voir schéma ci-dessous).

### `GET /api/inventory/component?id=`

Une fiche composant. `404` si absent.

### `POST /api/inventory/component`

Crée ou met à jour un composant. Corps : objet `Component` — `id` absent ou
`0` crée, sinon met à jour l'existant (ou le crée avec cet id s'il n'existe
pas). Réponse : la fiche enregistrée (avec son `id` définitif).

Si `category` est renseigné et ne correspond à aucune `Category` existante
(comparaison exacte du nom), elle est **créée automatiquement** — la fiche
composant garde un champ texte libre, mais la liste des catégories gérées
(`/api/inventory/categories`) reste toujours à jour sans action manuelle.

**Schéma `Component`** :

```jsonc
{
  "id": 12,
  "kind": "component",       // component | module | tool | consumable
  "name": "SSD1306",
  "reference": "SSD1306",
  "manufacturer": "Solomon Systech",
  "description": "Écran OLED I2C 0.96\"",
  "category": "Affichage",
  "subcategory": "",
  "type": "",
  "voltage": "3.3V",
  "current": "",
  "interfaceType": "I2C",
  "protocols": "",
  "i2cAddress": "0x3C",
  "frequency": "",
  "pinCount": 4,
  "compatibility": "",
  "price": 4.0,
  "supplier": "Temu",
  "purchaseDate": "2026-01-15",
  "receptionDate": "2026-01-22",
  "warranty": "",
  "state": "neuf",           // neuf | occasion | recupere | hs (libre)
  "status": "to_test",       // to_test | validating | validated | defective | archived
  "origin": "",
  "notes": "",
  "locationId": 3,
  "quantity": 6,
  "minStock": 2,
  "idealStock": 10
}
```

### `DELETE /api/inventory/component?id=`

Supprime un composant. `404` si absent. Ne supprime pas en cascade son
historique de mouvements ni ses documents rattachés (voir limites connues
dans `docs/ARCHITECTURE.md`).

### `GET /api/inventory/low-stock`

Composants dont `quantity < minStock` (et `minStock > 0`) — base de la
liste d'achats automatique. Même schéma `Component`.

## Inventaire — emplacements

### `GET /api/inventory/locations`

Liste de tous les emplacements : `{ "id", "name", "parentId" }`.
`parentId = 0` signifie racine. La hiérarchie se reconstruit côté client en
remontant les `parentId`.

### `POST /api/inventory/location`

Crée ou met à jour un emplacement. Corps : `{ "id", "name", "parentId" }`.

### `DELETE /api/inventory/location?id=`

Supprime un emplacement. Ne recase pas en cascade les composants ou
sous-emplacements qui le référencent (limite connue).

## Inventaire — catégories

Liste plate (pas de hiérarchie, contrairement aux emplacements). Une entrée
est créée automatiquement dès qu'une fiche composant est enregistrée avec
une catégorie inconnue (voir `POST /api/inventory/component` ci-dessus) —
ces routes servent surtout à renommer ou supprimer après coup.

### `GET /api/inventory/categories`

Liste de toutes les catégories : `{ "id", "name" }`.

### `POST /api/inventory/category`

Crée ou met à jour une catégorie. Corps : `{ "id", "name" }`.

### `DELETE /api/inventory/category?id=`

Supprime une catégorie. Ne modifie pas le champ `category` (texte libre)
des composants qui la référençaient encore par leur nom.

## Inventaire — mouvements de stock

### `GET /api/inventory/stock/history?componentId=`

Historique des mouvements d'un composant, du plus ancien au plus récent :

```jsonc
{ "id": 5, "componentId": 12, "type": "out", "quantity": 2, "timestamp": "2026-07-05T23:04:56Z", "note": "" }
```

`type` : `in` | `out` | `correction` | `inventory`. `in`/`out` modifient la
quantité de façon relative ; `correction`/`inventory` la fixent à la valeur
donnée.

### `POST /api/inventory/stock/movement`

Enregistre un mouvement et met à jour la quantité du composant. Corps :
`{ "componentId", "type", "quantity", "note" }`. Réponse : la fiche
composant après application du mouvement. `404` si le composant n'existe
pas.

## Documents

Module : `src/modules/document/`. Rattachables à un composant ou un projet.

### `GET /api/documents?ownerKind=&ownerId=`

`ownerKind` : `component` | `project`. Réponse :

```jsonc
{ "id": 1, "ownerKind": "component", "ownerId": 12, "title": "Datasheet SSD1306", "category": "datasheet", "url": "https://...", "notes": "" }
```

`category` (libre, valeurs suggérées côté UI) : `datasheet`, `manual`,
`schematic`, `pinout`, `link`, `other`. `url` est soit un lien externe, soit
un chemin LittleFS existant (voir `/api/files/*`) — aucun pipeline d'upload
dédié n'est dupliqué ici.

### `POST /api/documents`

Crée ou met à jour un document. Corps : objet ci-dessus.

### `DELETE /api/documents?id=`

Supprime un document.

## Projets

Module : `src/modules/project/`.

### `GET /api/projects`

Liste des projets : `{ "id", "name", "description", "version", "firmware", "gitRepo", "status", "notes" }`.
`status` libre (`in_progress`, `done`, `paused`, `archived` suggérés côté UI).

### `GET /api/project?id=`

Une fiche projet. `404` si absent.

### `POST /api/project`

Crée ou met à jour un projet.

### `DELETE /api/project?id=`

Supprime un projet **et sa nomenclature** (les lignes `ProjectComponent`
associées sont supprimées en cascade).

### `GET /api/projects/bom?projectId=`

Nomenclature complète du projet, avec disponibilité :

```jsonc
{
  "id": 3, "componentId": 12, "componentName": "SSD1306", "reference": "SSD1306",
  "inInventory": true, "unitPrice": 4.0,
  "quantityRequired": 2, "quantityAvailable": 6, "quantityMissing": 0
}
```

`inInventory` vaut `false` pour un élément **hors inventaire** (voir
`POST` ci-dessous) ou un composant supprimé depuis son ajout : dans ce cas
`componentId` est `0`, `quantityAvailable` est `0`, et la ligne est donc
toujours « à acheter » (`quantityMissing == quantityRequired`).

### `GET /api/projects/missing?projectId=`

Même schéma, filtré aux lignes où `quantityMissing > 0` — répond
directement à « quels composants me manquent pour ce projet ? ».

### `POST /api/projects/component`

Ajoute une ligne de nomenclature. Corps :
`{ "projectId", "componentId", "label", "quantityRequired" }`.

- `componentId != 0` : lie un composant de l'inventaire (`label` ignoré).
- `componentId == 0` : élément **hors inventaire** (pas encore acheté) ;
  `label` porte son nom libre. Toujours compté « à acheter » dans la
  nomenclature.

### `DELETE /api/projects/component?id=`

Retire une ligne de nomenclature (l'`id` est celui de la ligne, pas du
composant).

## Import / Export

Module : `src/modules/importexport/`. Voir aussi `docs/ARCHITECTURE.md` pour
le détail du traitement en mémoire (pas d'accès disque par ligne).

### `GET /api/inventory/export?format=native|bomist`

Télécharge un CSV (`text/csv`, séparateur `;`, en-têtes en première ligne).

- `native` : toutes les colonnes de `Component` (voir schéma plus haut) plus
  `location` (nom résolu, pas l'id). Réimportable tel quel ; le
  rapprochement à l'import se fait par `id`.
- `bomist` : colonnes `Stock;Part Number;Manufacturer;Description;Label;
  Storage;Unit Cost;Pref. Supplier;Type;Category;Worth`, compatibles avec
  un export [Bomist](https://bomist.com/). Mapping avec `Component` :
  `Stock`→`quantity`, `Part Number`→`reference`, `Label`→`subcategory`,
  `Storage`→`locationId` (résolu/créé par nom), `Unit Cost`/`Worth`→`price`
  (format `11,00 €`, virgule décimale).

Chaque export met à jour `lastExportAt` (et `lastBackupAt` si
`format=native`, seul format à fidélité complète).

### `POST /api/inventory/import?format=native|bomist`

Corps : le CSV brut (`text/csv; charset=utf-8`, UTF-8 requis). Rapprochement
par `id` (format natif) ou par référence/Part Number, insensible à la casse
(format Bomist) : une correspondance met à jour l'existant, sinon crée un
nouveau composant. Réponse :

```jsonc
{ "created": 23, "updated": 16, "failed": 0, "errors": ["ligne 4: Part Number manquant"] }
```

Met à jour `lastImportAt`.

### `GET /api/inventory/meta`

```jsonc
{ "lastImportAt": "2026-07-05T23:04:56Z", "lastExportAt": "", "lastBackupAt": "boot+42s" }
```

Chaîne vide = jamais effectué. Un préfixe `boot+<secondes>s` signifie que
l'horloge n'était pas synchronisée NTP au moment de l'action (repère relatif
au démarrage plutôt qu'un horodatage absolu).

## Routes héritées d'ESP32-Foundation

Fournies par `services/web_manager/`, `services/ota_manager/` et le module
optionnel `boot_log` — inchangées par rapport à la base, documentées ici
pour référence complète.

| Route | Rôle |
|---|---|
| `GET /api/system` | État système JSON (projet, version, build, uptime, heap, WiFi, LittleFS...), affiché sur `/system`. |
| `GET /api/logs`, `DELETE /api/logs` | Tampon de logs applicatifs JSON, affiché sur `/logs`. |
| `GET /api/files/list?path=`, `GET /api/files/download?path=`, `POST /api/files/upload`, `DELETE /api/files/delete?path=` | Gestionnaire de fichiers LittleFS, page `/files`. |
| `POST /api/ota/update` | Mise à jour firmware par upload web (page `/ota`). |
| `GET /api/bootlog`, `DELETE /api/bootlog` | Journal de redémarrage (module optionnel `ENABLE_BOOT_LOG`), page `/debug` — voir `docs/BOOT_LOG.md`. |
