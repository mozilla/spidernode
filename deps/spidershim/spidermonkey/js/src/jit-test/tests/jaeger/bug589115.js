// |jit-test| need-for-each

for each(y in ['', 0, '']) {
  y.lastIndexOf--
}

/* Don't assert/crash. */

