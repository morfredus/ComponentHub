(function () {
  var currentProject = null;
  var componentsById = {};
  var bomDisplayToId = {};   // "Nom (référence)" -> id inventaire (résolution de l'ajout)

  function escapeHtml(s) {
    return String(s == null ? '' : s).replace(/[&<>"]/g, function (c) {
      return ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c];
    });
  }

  function statusLabel(status) {
    return ({
      in_progress: 'En cours', done: 'Terminé', paused: 'En pause', archived: 'Archivé'
    })[status] || status || '';
  }

  function loadProjects() {
    return fetch('/api/projects').then(function (r) { return r.json(); }).then(function (list) {
      var tbody = document.querySelector('#projectTbl tbody');
      tbody.innerHTML = '';
      list.forEach(function (p) {
        var tr = document.createElement('tr');
        tr.innerHTML =
          '<td>' + escapeHtml(p.name) + '</td>' +
          '<td>' + escapeHtml(p.version) + '</td>' +
          '<td>' + escapeHtml(statusLabel(p.status)) + '</td>' +
          '<td class="row-actions">' +
            '<button data-action="view">Voir</button>' +
            '<button data-action="edit">Modifier</button>' +
            '<button data-action="delete">Supprimer</button>' +
          '</td>';
        tr.querySelector('[data-action=view]').addEventListener('click', function () { showDetail(p); });
        tr.querySelector('[data-action=edit]').addEventListener('click', function () { openProjectDialog(p); });
        tr.querySelector('[data-action=delete]').addEventListener('click', function () { deleteProject(p); });
        tbody.appendChild(tr);
      });
      document.getElementById('projectEmpty').hidden = list.length !== 0;
    });
  }

  function openProjectDialog(project) {
    var form = document.getElementById('projectForm');
    form.reset();
    var p = project || {};
    document.getElementById('projectDialogTitle').textContent = project ? 'Modifier le projet' : 'Nouveau projet';
    ['id', 'name', 'description', 'version', 'firmware', 'gitRepo', 'status', 'notes'].forEach(function (field) {
      var el = document.getElementById('p_' + field);
      var value = p[field];
      el.value = (value === undefined || value === null) ? (field === 'status' ? 'in_progress' : '') : value;
    });
    document.getElementById('projectDialog').showModal();
  }

  function saveProject(e) {
    e.preventDefault();
    var body = {
      id: parseInt(document.getElementById('p_id').value, 10) || 0,
      name: document.getElementById('p_name').value,
      description: document.getElementById('p_description').value,
      version: document.getElementById('p_version').value,
      firmware: document.getElementById('p_firmware').value,
      gitRepo: document.getElementById('p_gitRepo').value,
      status: document.getElementById('p_status').value,
      notes: document.getElementById('p_notes').value
    };
    fetch('/api/project', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body)
    }).then(function (r) { return r.json(); }).then(function (saved) {
      document.getElementById('projectDialog').close();
      loadProjects();
      if (currentProject && currentProject.id === saved.id) showDetail(saved);
    });
  }

  function deleteProject(p) {
    AppStatus.confirm('Supprimer le projet "' + p.name + '" (et sa nomenclature) ?', function () {
      fetch('/api/project?id=' + p.id, { method: 'DELETE' }).then(function () {
        if (currentProject && currentProject.id === p.id) {
          currentProject = null;
          document.getElementById('detailCard').hidden = true;
        }
        AppStatus.success('Projet "' + p.name + '" supprimé.');
        loadProjects();
      });
    });
  }

  function loadComponentOptions() {
    return fetch('/api/inventory/components').then(function (r) { return r.json(); }).then(function (list) {
      componentsById = {};
      bomDisplayToId = {};
      var dl = document.getElementById('bomComponentList');
      dl.innerHTML = '';
      list.forEach(function (c) {
        componentsById[c.id] = c;
        var display = c.name + (c.reference ? ' (' + c.reference + ')' : '');
        bomDisplayToId[display] = c.id;
        bomDisplayToId[c.name] = c.id;   // saisir juste le nom suffit
        var o = document.createElement('option');
        o.value = display;
        dl.appendChild(o);
      });
    });
  }

  function loadBom(projectId) {
    return fetch('/api/projects/bom?projectId=' + projectId).then(function (r) { return r.json(); }).then(function (list) {
      var tbody = document.querySelector('#bomTbl tbody');
      tbody.innerHTML = '';
      var toBuyLines = 0, toBuyUnits = 0;

      list.forEach(function (v) {
        var buy = v.quantityMissing > 0;
        if (buy) { toBuyLines++; toBuyUnits += v.quantityMissing; }

        var nameCell = escapeHtml(v.componentName) +
          (v.inInventory ? '' : ' <span class="muted">(hors inventaire)</span>');
        var missingCell = buy ? '<span class="badge err">' + v.quantityMissing + '</span>' : '0';
        var stateCell = buy
          ? '<span class="bom-state buy">🛒 À acheter</span>'
          : '<span class="bom-state ok">✅ Dispo</span>';

        var tr = document.createElement('tr');
        tr.innerHTML =
          '<td>' + nameCell + '</td>' +
          '<td>' + v.quantityRequired + '</td>' +
          '<td>' + v.quantityAvailable + '</td>' +
          '<td>' + missingCell + '</td>' +
          '<td>' + stateCell + '</td>' +
          '<td class="row-actions"><button data-action="remove">Retirer</button></td>';
        tr.querySelector('[data-action=remove]').addEventListener('click', function () {
          fetch('/api/projects/component?id=' + v.id, { method: 'DELETE' }).then(function () { loadBom(projectId); });
        });
        tbody.appendChild(tr);
      });

      var summary = document.getElementById('bomSummary');
      if (!list.length) {
        summary.textContent = '';
        summary.className = 'bom-summary';
      } else if (toBuyLines === 0) {
        summary.textContent = '✅ Tous les composants nécessaires sont disponibles en stock.';
        summary.className = 'bom-summary ok';
      } else {
        summary.textContent = '🛒 ' + toBuyLines + ' élément(s) à acheter — ' + toBuyUnits + ' unité(s) manquante(s).';
        summary.className = 'bom-summary buy';
      }
    });
  }

  function showDetail(project) {
    currentProject = project;
    document.getElementById('detailCard').hidden = false;
    document.getElementById('detailName').textContent = project.name;
    document.getElementById('detailDescription').textContent = project.description || '';
    loadComponentOptions().then(function () { loadBom(project.id); });
  }

  document.getElementById('bomAddForm').addEventListener('submit', function (e) {
    e.preventDefault();
    if (!currentProject) return;
    var input = document.getElementById('bomComponentInput');
    var text = input.value.trim();
    if (!text) return;

    // Texte correspondant à un composant de l'inventaire -> on le lie par id.
    // Sinon -> élément hors inventaire (componentId 0 + libellé), « à acheter ».
    var componentId = bomDisplayToId[text] || 0;
    var body = {
      projectId: currentProject.id,
      componentId: componentId,
      label: componentId ? '' : text,
      quantityRequired: parseInt(document.getElementById('bomQuantity').value, 10) || 1
    };
    fetch('/api/projects/component', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body)
    }).then(function (r) {
      if (!r.ok) { AppStatus.error("Échec de l'ajout au projet (HTTP " + r.status + ")."); return; }
      input.value = '';
      document.getElementById('bomQuantity').value = '1';
      loadBom(currentProject.id);
    }).catch(function () {
      AppStatus.error("Échec réseau lors de l'ajout au projet.");
    });
  });

  // --- Impression / export PDF de la nomenclature ---
  function fmtPrice(v) { return (v || 0).toFixed(2).replace('.', ',') + ' €'; }

  function buildPrintHtml(project, bom) {
    var toBuy = bom.filter(function (v) { return v.quantityMissing > 0; });
    var now = new Date().toLocaleString('fr-FR');
    function extLabel(v) { return v.inInventory ? '' : ' (hors inventaire)'; }

    var bomRows = bom.map(function (v) {
      return '<tr><td>' + escapeHtml(v.componentName) + extLabel(v) + '</td>' +
        '<td>' + escapeHtml(v.reference || '') + '</td>' +
        '<td class="num">' + v.quantityRequired + '</td>' +
        '<td class="num">' + v.quantityAvailable + '</td>' +
        '<td class="num">' + v.quantityMissing + '</td></tr>';
    }).join('');

    var toBuyCost = 0;
    var buyRows = toBuy.map(function (v) {
      var lineCost = (v.unitPrice || 0) * v.quantityMissing;
      toBuyCost += lineCost;
      return '<tr><td>' + escapeHtml(v.componentName) + extLabel(v) + '</td>' +
        '<td>' + escapeHtml(v.reference || '') + '</td>' +
        '<td class="num">' + v.quantityMissing + '</td>' +
        '<td class="num">' + (v.unitPrice ? fmtPrice(v.unitPrice) : '—') + '</td>' +
        '<td class="num">' + (v.unitPrice ? fmtPrice(lineCost) : '—') + '</td></tr>';
    }).join('');

    var buySection = toBuy.length
      ? '<h2>Reste à acheter</h2>' +
        '<table><thead><tr><th>Élément</th><th>Référence</th><th class="num">Qté</th><th class="num">Prix unit.</th><th class="num">Total</th></tr></thead>' +
        '<tbody>' + buyRows + '</tbody></table>' +
        '<p class="total">Coût estimé (éléments à prix connu) : <strong>' + fmtPrice(toBuyCost) + '</strong></p>'
      : '<p class="ok">&#10003; Tous les composants nécessaires sont disponibles en stock.</p>';

    return '<!DOCTYPE html><html lang="fr"><head><meta charset="UTF-8"><title>' + escapeHtml(project.name) + ' — Nomenclature</title>' +
      '<style>body{font-family:system-ui,sans-serif;color:#000;margin:2rem;}h1{margin:0 0 0.2rem;}h2{margin:1.4rem 0 0.5rem;}' +
      '.meta{color:#555;font-size:0.85rem;margin-bottom:1rem;}table{width:100%;border-collapse:collapse;font-size:0.85rem;margin-bottom:0.5rem;}' +
      'th,td{border:1px solid #ccc;padding:0.35rem 0.5rem;text-align:left;}th.num,td.num{text-align:right;}' +
      '.total{font-size:0.9rem;}.ok{color:#137333;font-weight:600;}@media print{body{margin:0.5cm;}}</style></head><body>' +
      '<h1>' + escapeHtml(project.name) + '</h1>' +
      '<div class="meta">' + (project.version ? 'Version ' + escapeHtml(project.version) + ' — ' : '') +
      'Nomenclature imprimée le ' + escapeHtml(now) + '</div>' +
      '<h2>Composants nécessaires</h2>' +
      '<table><thead><tr><th>Élément</th><th>Référence</th><th class="num">Requis</th><th class="num">Dispo.</th><th class="num">Manquant</th></tr></thead>' +
      '<tbody>' + bomRows + '</tbody></table>' +
      buySection + '</body></html>';
  }

  document.getElementById('printBomBtn').addEventListener('click', function () {
    if (!currentProject) return;
    fetch('/api/projects/bom?projectId=' + currentProject.id).then(function (r) { return r.json(); }).then(function (bom) {
      var w = window.open('', '_blank');
      if (!w) { AppStatus.error("Impossible d'ouvrir la fenêtre d'impression (popup bloqué ?)."); return; }
      w.document.write(buildPrintHtml(currentProject, bom));
      w.document.close();
      w.focus();
      w.print();
    });
  });

  // --- Documentation du projet (même API que les composants, ownerKind=project) ---
  function loadDocuments(projectId) {
    return fetch('/api/documents?ownerKind=project&ownerId=' + projectId)
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
            fetch('/api/documents?id=' + d.id, { method: 'DELETE' }).then(function () { loadDocuments(projectId); });
          });
          ul.appendChild(li);
        });
      });
  }

  document.getElementById('documentsBtn').addEventListener('click', function () {
    if (!currentProject) return;
    document.getElementById('documentOwnerName').textContent = currentProject.name;
    document.getElementById('d_ownerId').value = currentProject.id;
    document.getElementById('d_title').value = '';
    document.getElementById('d_url').value = '';
    document.getElementById('d_notes').value = '';
    loadDocuments(currentProject.id);
    document.getElementById('documentDialog').showModal();
  });

  document.getElementById('documentCloseBtn').addEventListener('click', function () { document.getElementById('documentDialog').close(); });
  document.getElementById('documentAddForm').addEventListener('submit', function (e) {
    e.preventDefault();
    var ownerId = parseInt(document.getElementById('d_ownerId').value, 10);
    var body = {
      ownerKind: 'project',
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

  document.getElementById('addProjectBtn').addEventListener('click', function () { openProjectDialog(null); });
  document.getElementById('projectCancelBtn').addEventListener('click', function () { document.getElementById('projectDialog').close(); });
  document.getElementById('projectForm').addEventListener('submit', saveProject);

  loadProjects();
})();
