(function () {
var kFirstSurrogate = 0xD800;
var kReplacementCharacter = 0XFFFD;
var kNumCodepoints = 0x10000;
var kNumSurrogates = 0x800;

function TestDirectRange(txt) {
  for (var i = 0; i < kFirstSurrogate; ++i) {
    var ch = txt.charCodeAt(i);
    if (ch !== i) {
      puts("TestDirectRange: got codepoint " + ch + ", expected " + i);
      return false;
    }
  }
  return true;
}

function TestSurrogateRange(txt, count) {
  var last_surrogate_index = kFirstSurrogate + count;
  for (var i = kFirstSurrogate; i < last_surrogate_index; ++i) {
    var ch = txt.charCodeAt(i);
    if (ch !== kReplacementCharacter) {
      puts("TestSurrogateRange: surrogate not replaced at " + i);
      return false;
    }
  }
  return true;
}

function TestOffsetRange(txt, first) {
  var offset = first - kFirstSurrogate;
  for (var i = first; i < txt.length; ++i) {
    var ch = txt.charCodeAt(i);
    var expected = i - offset + kNumSurrogates;
    if (ch !== expected) {
      puts("TestOffsetRange: got codepoint " + ch + ", exepected " + expected);
      return false;
    }
  }
  return true;
}

function TestGoodCodepoints(txt) {
  var kExpectedLength = kNumCodepoints - kNumSurrogates;
  if (txt.length !== kExpectedLength) {
    puts('TestGoodCodepoints: received length ' + txt.length + 
	 "expected " + kExpectedLength);
    return false;
  }
  return TestDirectRange(txt) && 
    TestOffsetRange(txt, kFirstSurrogate);
}

function TestAllCodepoints(txt) {
  // Webkit replaces each byte of a surrogate codepoint, whereas Gecko
  // does the more obvious thing.
  var kWebkitExpectedLength = kNumCodepoints + 2*kNumSurrogates;
  var kGeckoExpectedLength = kNumCodepoints;
  var surrogate_count = kNumSurrogates;
  if (txt.length === kWebkitExpectedLength) {
    puts('TestAllCodepoints: WebKit-style surrogate replacement.');
    surrogate_count *= 3;
  } else if (txt.length === kGeckoExpectedLength) {
    puts('TestAllCodepoints: Gecko-style surrogate replacement.');
  } else {
    puts('TestAllCodepoints: unexpected length ' + txt.length);
    return false;
  }
  return TestDirectRange(txt) &&
    TestSurrogateRange(txt, surrogate_count) &&
    TestOffsetRange(txt, kFirstSurrogate + surrogate_count);
}

XHRTest('good_codepoints.utf8', TestGoodCodepoints);
XHRTest('all_codepoints.utf8', TestAllCodepoints);

})();