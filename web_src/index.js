(function () {
  function escapeHtml(s) {
    return String(s == null ? '' : s).replace(/[&<>"]/g, function (c) {
      return ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' })[c];
    });
  }

  function fmtPrice(value) {
    return value.toFixed(2).replace('.', ',') + ' €';
  }

  function fmtDate(iso) {
    if (!iso) return 'jamais';
    if (iso.indexOf('boot+') === 0) return 'il y a ' + iso.slice(5) + ' (horloge non synchronisée)';
    var d = new Date(iso);
    if (isNaN(d.getTime())) return iso;
    return d.toLocaleString('fr-FR', { day: '2-digit', month: '2-digit', year: 'numeric', hour: '2-digit', minute: '2-digit' });
  }

  function statTile(value, label, href, warn, small) {
    var tag = href ? 'a' : 'div';
    var cls = 'stat-tile' + (warn ? ' warn' : '') + (small ? ' text' : '');
    var hrefAttr = href ? ' href="' + href + '"' : '';
    return '<' + tag + ' class="' + cls + '"' + hrefAttr + '>' +
      '<div class="stat-value">' + escapeHtml(String(value)) + '</div>' +
      '<div class="stat-label">' + escapeHtml(label) + '</div>' +
      '</' + tag + '>';
  }

  // N'importe laquelle de ces routes peut être absente (firmware pas encore
  // à jour) ou en échec transitoire : chaque appel a son propre repli, pour
  // qu'une seule route en échec n'empêche jamais l'affichage du reste du
  // tableau de bord (voir historique : Promise.all seul faisait échouer tout
  // l'affichage dès qu'une route manquait).
  function fetchJsonSafe(url, fallback) {
    return fetch(url).then(function (r) {
      if (!r.ok) return fallback;
      return r.json().catch(function () { return fallback; });
    }).catch(function () { return fallback; });
  }

  Promise.all([
    fetchJsonSafe('/api/inventory/components', []),
    fetchJsonSafe('/api/inventory/locations', []),
    fetchJsonSafe('/api/inventory/low-stock', []),
    fetchJsonSafe('/api/projects', []),
    fetchJsonSafe('/api/system', {}),
    fetchJsonSafe('/api/inventory/meta', {})
  ]).then(function (results) {
    var components = results[0];
    var locations = results[1];
    var lowStock = results[2];
    var projects = results[3];
    var meta = results[5];

    var counts = { component: 0, module: 0, tool: 0, consumable: 0 };
    var totalQuantity = 0;
    var totalValue = 0;
    var toTestCount = 0;
    components.forEach(function (c) {
      counts[c.kind] = (counts[c.kind] || 0) + 1;
      totalQuantity += c.quantity || 0;
      totalValue += (c.price || 0) * (c.quantity || 0);
      if (c.status === 'to_test') toTestCount++;
    });

    var shortfallUnits = lowStock.reduce(function (sum, c) {
      return sum + Math.max(0, (c.minStock || 0) - (c.quantity || 0));
    }, 0);

    var projectsInProgress = projects.filter(function (p) { return p.status === 'in_progress'; }).length;

    var tiles = [
      statTile(components.length, '📦 Nombre total de références', '/inventory'),
      statTile(totalQuantity, '📦 Nombre total de pièces', '/inventory'),
      statTile(counts.component || 0, 'Composants', '/inventory?kind=component'),
      statTile(counts.module || 0, 'Modules', '/inventory?kind=module'),
      statTile(counts.tool || 0, 'Outils', '/inventory?kind=tool'),
      statTile(counts.consumable || 0, 'Consommables', '/inventory?kind=consumable'),
      statTile(fmtPrice(totalValue), 'Valeur totale'),
      statTile(lowStock.length, '⚠️ Sous le seuil minimum', '/inventory?lowStock=1', lowStock.length > 0),
      statTile(shortfallUnits, '🛒 Liste d\'achats en attente (unités)', '/inventory?lowStock=1', shortfallUnits > 0),
      statTile(toTestCount, '🧪 Composants "À tester"', '/inventory?status=to_test', toTestCount > 0),
      statTile(locations.length, 'Emplacements', '/locations'),
      statTile(projects.length, '📁 Nombre de projets (' + projectsInProgress + ' en cours)', '/projects'),
      statTile(fmtDate(meta.lastBackupAt), '💾 Date de la dernière sauvegarde', '/import-export', false, true),
      statTile(fmtDate(meta.lastImportAt), '📥 Dernier import', '/import-export', false, true),
      statTile(fmtDate(meta.lastExportAt), '📤 Dernier export', '/import-export', false, true)
    ];
    document.getElementById('statGrid').innerHTML = tiles.join('');

    if (lowStock.length) {
      document.getElementById('lowStockCard').hidden = false;
      var list = document.getElementById('lowStockList');
      list.innerHTML = '';
      lowStock.slice(0, 8).forEach(function (c) {
        var li = document.createElement('li');
        li.innerHTML = '<span>' + escapeHtml(c.name) + '</span><span>' + c.quantity + ' / ' + c.minStock + '</span>';
        list.appendChild(li);
      });
    }
  }).catch(function () {
    if (window.AppStatus) AppStatus.error("Impossible de charger l'état de l'inventaire.");
  });
})();
