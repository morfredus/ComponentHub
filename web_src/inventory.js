(function () {
  var locationsById = {};
  var lastComponents = [];

  function locationPath(id) {
    var parts = [];
    var guard = 0;
    while (id && locationsById[id] && guard++ < 20) {
      parts.unshift(locationsById[id].name);
      id = locationsById[id].parentId;
    }
    return parts.join(' > ');
  }

  function loadLocations() {
    return fetch('/api/inventory/locations').then(function (r) { return r.json(); }).then(function (list) {
      locationsById = {};
      list.forEach(function (l) { locationsById[l.id] = l; });

      // Champ texte + datalist plutôt qu'un <select> : on peut choisir un
      // emplacement existant (par son chemin complet) ou en taper un nouveau,
      // créé à la volée à l'enregistrement (voir resolveLocationId).
      var dl = document.getElementById('locationDatalist');
      dl.innerHTML = '';
      list.forEach(function (l) {
        var o = document.createElement('option');
        o.value = locationPath(l.id);
        dl.appendChild(o);
      });
    });
  }

  // Résout un chemin d'emplacement tapé/choisi ("Atelier > Armoire A") vers
  // son id existant, ou crée un nouvel emplacement racine à la volée si ce
  // chemin ne correspond à rien de connu (pas de sélecteur de parent dans ce
  // flux rapide — utiliser la page Emplacements pour une hiérarchie précise).
  function resolveLocationId(text) {
    text = (text || '').trim();
    if (!text) return Promise.resolve(0);

    var matchId = 0;
    Object.keys(locationsById).forEach(function (id) {
      if (locationPath(Number(id)) === text) matchId = Number(id);
    });
    if (matchId) return Promise.resolve(matchId);

    return fetch('/api/inventory/location', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ id: 0, name: text, parentId: 0 })
    }).then(function (r) { return r.json(); }).then(function (loc) { return loc.id; });
  }

  function loadCategories() {
    return fetch('/api/inventory/categories').then(function (r) { return r.json(); }).then(function (list) {
      var dl = document.getElementById('categoryList');
      dl.innerHTML = '';
      list.forEach(function (c) {
        var o = document.createElement('option');
        o.value = c.name;
        dl.appendChild(o);
      });
    });
  }

  function statusLabel(status) {
    return ({
      to_test: 'À tester', validating: 'En validation', validated: 'Validé',
      defective: 'Défectueux', archived: 'Archivé'
    })[status] || status || '';
  }

  // Icônes façon signalisation plutôt qu'un texte de statut : lecture plus
  // rapide dans une colonne étroite. Le libellé complet reste accessible via
  // l'attribut title (survol) plutôt que perdu.
  function statusIcon(status) {
    return ({
      to_test: '🟡', validating: '🚧', validated: '🟢',
      defective: '🛑', archived: '📦'
    })[status] || '❔';
  }

  function kindLabel(kind) {
    return ({ component: 'Composant', module: 'Module', tool: 'Outil', consumable: 'Consommable' })[kind] || kind || '';
  }

  function closeAllRowMenus(except) {
    document.querySelectorAll('.row-menu[open]').forEach(function (d) {
      if (d !== except) d.removeAttribute('open');
    });
  }

  function renderRow(c) {
    var tr = document.createElement('tr');
    var lowStock = c.minStock > 0 && c.quantity < c.minStock;
    var qtyBadge = lowStock ? '<span class="badge err">' + c.quantity + '</span>' : c.quantity;

    tr.innerHTML =
      '<td>' + escapeHtml(kindLabel(c.kind)) + '</td>' +
      '<td>' + escapeHtml(c.name) + '</td>' +
      '<td>' + escapeHtml(c.reference) + '</td>' +
      '<td>' + escapeHtml(c.manufacturer) + '</td>' +
      '<td>' + escapeHtml(c.category) + '</td>' +
      '<td>' + qtyBadge + '</td>' +
      '<td>' + escapeHtml(locationPath(c.locationId)) + '</td>' +
      '<td title="' + escapeHtml(statusLabel(c.status)) + '">' + statusIcon(c.status) + '</td>' +
      '<td class="row-actions">' +
        '<details class="row-menu">' +
          '<summary>&#8942;</summary>' +
          '<div class="row-menu-list">' +
            '<button type="button" data-action="edit">Modifier</button>' +
            '<button type="button" data-action="move">Mouvement</button>' +
            '<button type="button" data-action="docs">Documents</button>' +
            '<button type="button" data-action="photos">Photos</button>' +
            '<button type="button" data-action="qrcode">QR Code</button>' +
            '<button type="button" data-action="duplicate">Dupliquer</button>' +
            '<button type="button" class="danger" data-action="delete">Supprimer</button>' +
          '</div>' +
        '</details>' +
      '</td>';

    var menu = tr.querySelector('.row-menu');
    menu.addEventListener('toggle', function () {
      if (!menu.open) { menu.classList.remove('drop-up'); return; }
      closeAllRowMenus(menu);
      // Ouvre le menu vers le haut s'il n'y a pas assez de place en dessous
      // (dernières lignes) : évite qu'il déborde en bas de page / soit rogné
      // par le conteneur défilant du tableau.
      var list = menu.querySelector('.row-menu-list');
      var spaceBelow = window.innerHeight - menu.getBoundingClientRect().bottom;
      menu.classList.toggle('drop-up', list.offsetHeight + 8 > spaceBelow);
    });

    function onAction(action, handler) {
      tr.querySelector('[data-action=' + action + ']').addEventListener('click', function () {
        menu.removeAttribute('open');
        handler();
      });
    }
    onAction('edit', function () { openComponentDialog(c); });
    onAction('move', function () { openMovementDialog(c); });
    onAction('docs', function () { openDocumentDialog(c); });
    onAction('photos', function () { AppStatus.info('Photos : bientôt disponible.'); });
    onAction('qrcode', function () { AppStatus.info('QR Code : bientôt disponible.'); });
    onAction('duplicate', function () { duplicateComponent(c); });
    onAction('delete', function () { deleteComponent(c); });
    return tr;
  }

  function escapeHtml(s) {
    return String(s == null ? '' : s).replace(/[&<>"]/g, function (c) {
      return ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c];
    });
  }

  function loadComponents() {
    var params = new URLSearchParams();
    var q = document.getElementById('q').value.trim();
    var kind = document.getElementById('kindFilter').value;
    var category = document.getElementById('categoryFilter').value.trim();
    var status = document.getElementById('statusFilter').value;
    var lowStock = document.getElementById('lowStockFilter').checked;
    if (q) params.set('q', q);
    if (kind) params.set('kind', kind);
    if (category) params.set('category', category);
    if (status) params.set('status', status);
    if (lowStock) params.set('lowStock', '1');

    return fetch('/api/inventory/components?' + params.toString())
      .then(function (r) { return r.json(); })
      .then(function (list) {
        lastComponents = list;
        var tbody = document.querySelector('#tbl tbody');
        tbody.innerHTML = '';
        list.forEach(function (c) { tbody.appendChild(renderRow(c)); });
        document.getElementById('empty').hidden = list.length !== 0;
      });
  }

  function refresh() {
    return Promise.all([loadLocations(), loadCategories()]).then(loadComponents);
  }

  function fillComponentForm(component, keepContext) {
    var form = document.getElementById('componentForm');
    form.reset();
    var c = component || keepContext || {};
    document.getElementById('componentDialogTitle').textContent = component ? 'Modifier le composant' : 'Nouveau composant';
    [
      'id', 'kind', 'name', 'reference', 'manufacturer', 'description', 'category', 'subcategory', 'type',
      'voltage', 'current', 'interfaceType', 'protocols', 'i2cAddress', 'frequency', 'pinCount',
      'compatibility', 'price', 'supplier', 'purchaseDate', 'receptionDate', 'warranty',
      'state', 'status', 'origin', 'quantity', 'minStock', 'idealStock', 'notes'
    ].forEach(function (field) {
      var el = document.getElementById('f_' + field);
      if (!el) return;
      var value = c[field];
      var defaults = { status: 'to_test', kind: 'component' };
      el.value = (value === undefined || value === null) ? (defaults[field] || '') : value;
    });
    // Emplacement : champ texte affichant le chemin complet, pas l'id brut.
    document.getElementById('f_locationId').value = c.locationId ? locationPath(c.locationId) : '';
    document.getElementById('f_salvageMode').checked = !!keepContext;
  }

  function openComponentDialog(component) {
    fillComponentForm(component, null);
    document.getElementById('componentDialog').showModal();
  }

  function collectComponentForm() {
    return {
      id: parseInt(document.getElementById('f_id').value, 10) || 0,
      kind: document.getElementById('f_kind').value,
      name: document.getElementById('f_name').value,
      reference: document.getElementById('f_reference').value,
      manufacturer: document.getElementById('f_manufacturer').value,
      description: document.getElementById('f_description').value,
      category: document.getElementById('f_category').value,
      subcategory: document.getElementById('f_subcategory').value,
      type: document.getElementById('f_type').value,
      voltage: document.getElementById('f_voltage').value,
      current: document.getElementById('f_current').value,
      interfaceType: document.getElementById('f_interfaceType').value,
      protocols: document.getElementById('f_protocols').value,
      i2cAddress: document.getElementById('f_i2cAddress').value,
      frequency: document.getElementById('f_frequency').value,
      pinCount: parseInt(document.getElementById('f_pinCount').value, 10) || 0,
      compatibility: document.getElementById('f_compatibility').value,
      price: parseFloat(document.getElementById('f_price').value) || 0,
      supplier: document.getElementById('f_supplier').value,
      purchaseDate: document.getElementById('f_purchaseDate').value,
      receptionDate: document.getElementById('f_receptionDate').value,
      warranty: document.getElementById('f_warranty').value,
      state: document.getElementById('f_state').value,
      status: document.getElementById('f_status').value,
      origin: document.getElementById('f_origin').value,
      quantity: parseInt(document.getElementById('f_quantity').value, 10) || 0,
      minStock: parseInt(document.getElementById('f_minStock').value, 10) || 0,
      idealStock: parseInt(document.getElementById('f_idealStock').value, 10) || 0,
      notes: document.getElementById('f_notes').value
    };
  }

  function saveComponent(e) {
    e.preventDefault();
    var salvageMode = document.getElementById('f_salvageMode').checked;
    var locationText = document.getElementById('f_locationId').value;

    resolveLocationId(locationText).then(function (locationId) {
      var payload = collectComponentForm();
      payload.locationId = locationId;

      return fetch('/api/inventory/component', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      }).then(function () { return payload; });
    }).then(function (payload) {
      refresh();
      if (salvageMode) {
        // Récupération en série : on garde provenance/emplacement/état pour le composant suivant.
        fillComponentForm(null, {
          kind: payload.kind, state: payload.state, status: payload.status,
          origin: payload.origin, locationId: payload.locationId
        });
        document.getElementById('f_name').focus();
      } else {
        document.getElementById('componentDialog').close();
      }
    });
  }

  function duplicateComponent(c) {
    var copy = Object.assign({}, c);
    copy.id = 0;
    if (copy.name) copy.name = copy.name + ' (copie)';
    fillComponentForm(copy, null);
    document.getElementById('componentDialogTitle').textContent = 'Dupliquer le composant';
    document.getElementById('componentDialog').showModal();
  }

  function deleteComponent(c) {
    AppStatus.confirm('Supprimer "' + c.name + '" ?', function () {
      fetch('/api/inventory/component?id=' + c.id, { method: 'DELETE' }).then(function () {
        AppStatus.success('"' + c.name + '" supprimé.');
        refresh();
      });
    });
  }

  var currentMovementComponent = null;

  function loadHistory(componentId) {
    return fetch('/api/inventory/stock/history?componentId=' + componentId)
      .then(function (r) { return r.json(); })
      .then(function (list) {
        var ul = document.getElementById('movementHistory');
        ul.innerHTML = '';
        list.slice().reverse().forEach(function (m) {
          var li = document.createElement('li');
          li.textContent = m.timestamp + ' — ' + m.type + ' ' + m.quantity + (m.note ? ' (' + m.note + ')' : '');
          ul.appendChild(li);
        });
      });
  }

  function openMovementDialog(component) {
    currentMovementComponent = component;
    document.getElementById('movementComponentName').textContent = component.name;
    document.getElementById('m_componentId').value = component.id;
    document.getElementById('m_quantity').value = '';
    document.getElementById('m_note').value = '';
    loadHistory(component.id);
    document.getElementById('movementDialog').showModal();
  }

  function saveMovement(e) {
    e.preventDefault();
    var body = {
      componentId: parseInt(document.getElementById('m_componentId').value, 10),
      type: document.getElementById('m_type').value,
      quantity: parseInt(document.getElementById('m_quantity').value, 10) || 0,
      note: document.getElementById('m_note').value
    };
    fetch('/api/inventory/stock/movement', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body)
    }).then(function () {
      document.getElementById('m_quantity').value = '';
      document.getElementById('m_note').value = '';
      loadHistory(body.componentId);
      loadComponents();
    });
  }

  function loadDocuments(componentId) {
    return fetch('/api/documents?ownerKind=component&ownerId=' + componentId)
      .then(function (r) { return r.json(); })
      .then(function (list) {
        var ul = document.getElementById('documentList');
        ul.innerHTML = '';
        list.forEach(function (d) {
          var li = document.createElement('li');
          li.innerHTML = '<strong>' + escapeHtml(d.category) + '</strong> — ' +
            '<a href="' + escapeHtml(d.url) + '" target="_blank" rel="noopener">' + escapeHtml(d.title) + '</a> ' +
            '<button type="button" data-id="' + d.id + '">supprimer</button>';
          li.querySelector('button').addEventListener('click', function () {
            fetch('/api/documents?id=' + d.id, { method: 'DELETE' }).then(function () { loadDocuments(componentId); });
          });
          ul.appendChild(li);
        });
      });
  }

  function openDocumentDialog(component) {
    document.getElementById('documentOwnerName').textContent = component.name;
    document.getElementById('d_ownerId').value = component.id;
    document.getElementById('d_title').value = '';
    document.getElementById('d_url').value = '';
    document.getElementById('d_notes').value = '';
    loadDocuments(component.id);
    document.getElementById('documentDialog').showModal();
  }

  document.getElementById('documentCloseBtn').addEventListener('click', function () { document.getElementById('documentDialog').close(); });
  document.getElementById('documentAddForm').addEventListener('submit', function (e) {
    e.preventDefault();
    var ownerId = parseInt(document.getElementById('d_ownerId').value, 10);
    var body = {
      ownerKind: 'component',
      ownerId: ownerId,
      title: document.getElementById('d_title').value,
      category: document.getElementById('d_category').value,
      url: document.getElementById('d_url').value,
      notes: document.getElementById('d_notes').value
    };
    fetch('/api/documents', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body)
    }).then(function () {
      document.getElementById('d_title').value = '';
      document.getElementById('d_url').value = '';
      document.getElementById('d_notes').value = '';
      loadDocuments(ownerId);
    });
  });

  document.getElementById('addBtn').addEventListener('click', function () { openComponentDialog(null); });
  document.getElementById('cancelBtn').addEventListener('click', function () { document.getElementById('componentDialog').close(); });
  document.getElementById('componentForm').addEventListener('submit', saveComponent);

  document.getElementById('movementCancelBtn').addEventListener('click', function () { document.getElementById('movementDialog').close(); });
  document.getElementById('movementForm').addEventListener('submit', saveMovement);

  var searchTimer;
  ['q', 'categoryFilter'].forEach(function (id) {
    document.getElementById(id).addEventListener('input', function () {
      clearTimeout(searchTimer);
      searchTimer = setTimeout(loadComponents, 250);
    });
  });
  document.getElementById('lowStockFilter').addEventListener('change', loadComponents);
  document.getElementById('kindFilter').addEventListener('change', loadComponents);
  document.getElementById('statusFilter').addEventListener('change', loadComponents);

  // Permet un lien direct depuis le tableau de bord (Labo) : préremplit
  // les filtres et/ou ouvre directement le formulaire d'ajout.
  var params = new URLSearchParams(location.search);
  if (params.get('lowStock') === '1') document.getElementById('lowStockFilter').checked = true;
  if (params.has('kind')) document.getElementById('kindFilter').value = params.get('kind');
  if (params.has('status')) document.getElementById('statusFilter').value = params.get('status');

  refresh().then(function () {
    if (params.get('action') === 'add') openComponentDialog(null);
  });

  // Ferme tout menu ⋮ ouvert au clic en dehors (les <details> ne le font pas nativement).
  document.addEventListener('click', function (e) {
    document.querySelectorAll('.row-menu[open]').forEach(function (d) {
      if (!d.contains(e.target)) d.removeAttribute('open');
    });
  });
})();
