var out = window.document.getElementById('test_output');

function puts(str) {
  out.innerHTML += str + '<br>';
}

function XHRTest(url, cb) {
  var req = new XMLHttpRequest();
  req.onload = function() {
    if (this.status === 200 || this.status === 0) {
      puts('Test: ' + cb.name);
      if (cb(this.responseText)) {
	puts('all tests pass!');
      }
    }
    else {
      puts('XHR: failed to download ' + url + ' with status ' + this.status);
    }
  }
  req.open('GET', url, true);
  req.send(null);
}
