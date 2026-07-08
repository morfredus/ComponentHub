(function () {
  var FILENAMES = { native: 'componenthub_export.csv', bomist: 'bomist_export.csv' };
  var FORMAT_LABELS = { native: 'ComponentHub', bomist: 'Bomist' };

  function download(format) {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', '/api/inventory/export?format=' + format);
    xhr.responseType = 'blob';

    AppStatus.progress(0, 'Export ' + (FORMAT_LABELS[format] || format) + ' en cours...');

    xhr.onprogress = function (e) {
      if (e.lengthComputable) {
        AppStatus.progress(e.loaded / e.total, 'Export en cours... (' + Math.round(e.loaded / 1024) + ' Ko)');
      }
    };
    xhr.onload = function () {
      if (xhr.status !== 200) { AppStatus.error("Échec de l'export (HTTP " + xhr.status + ")"); return; }
      var url = URL.createObjectURL(xhr.response);
      var a = document.createElement('a');
      a.href = url;
      a.download = FILENAMES[format] || 'export.csv';
      document.body.appendChild(a);
      a.click();
      a.remove();
      setTimeout(function () { URL.revokeObjectURL(url); }, 2000);
      AppStatus.success('Export ' + (FORMAT_LABELS[format] || format) + ' terminé.');
    };
    xhr.onerror = function () { AppStatus.error("Échec réseau pendant l'export."); };
    xhr.send();
  }

  document.getElementById('exportNativeBtn').addEventListener('click', function () { download('native'); });
  document.getElementById('exportBomistBtn').addEventListener('click', function () { download('bomist'); });

  document.getElementById('importForm').addEventListener('submit', function (e) {
    e.preventDefault();
    var file = document.getElementById('importFile').files[0];
    if (!file) return;

    var format = document.getElementById('importFormat').value;
    var errorsList = document.getElementById('importErrors');
    errorsList.innerHTML = '';

    var reader = new FileReader();
    reader.onload = function () {
      var xhr = new XMLHttpRequest();
      xhr.open('POST', '/api/inventory/import?format=' + format);
      xhr.setRequestHeader('Content-Type', 'text/csv; charset=utf-8');

      AppStatus.progress(0, 'Envoi du fichier...');
      xhr.upload.onprogress = function (e) {
        if (e.lengthComputable) {
          AppStatus.progress(e.loaded / e.total, 'Envoi du fichier... (' + Math.round((e.loaded / e.total) * 100) + '%)');
        }
      };
      xhr.onload = function () {
        if (xhr.status !== 200) { AppStatus.error("Échec de l'import (HTTP " + xhr.status + ")"); return; }
        var result;
        try { result = JSON.parse(xhr.responseText); }
        catch (err) { AppStatus.error('Réponse invalide du serveur.'); return; }

        AppStatus.success(result.created + ' créé(s), ' + result.updated + ' mis à jour, ' + result.failed + ' échec(s).');
        (result.errors || []).forEach(function (msg) {
          var li = document.createElement('li');
          li.textContent = msg;
          errorsList.appendChild(li);
        });
      };
      xhr.onerror = function () { AppStatus.error("Échec réseau pendant l'import."); };
      xhr.send(reader.result);
    };
    reader.onerror = function () { AppStatus.error('Impossible de lire le fichier sélectionné.'); };
    reader.readAsText(file, 'utf-8');
  });
})();
