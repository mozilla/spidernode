'use strict';
// test compressing and uncompressing a string with zlib

require('../common');
const assert = require('assert');
const zlib = require('zlib');

const inputString = 'ΩΩLorem ipsum dolor sit amet, consectetur adipiscing eli' +
                    't. Morbi faucibus, purus at gravida dictum, libero arcu ' +
                    'convallis lacus, in commodo libero metus eu nisi. Nullam' +
                    ' commodo, neque nec porta placerat, nisi est fermentum a' +
                    'ugue, vitae gravida tellus sapien sit amet tellus. Aenea' +
                    'n non diam orci. Proin quis elit turpis. Suspendisse non' +
                    ' diam ipsum. Suspendisse nec ullamcorper odio. Vestibulu' +
                    'm arcu mi, sodales non suscipit id, ultrices ut massa. S' +
                    'ed ac sem sit amet arcu malesuada fermentum. Nunc sed. ';
const expectedBase64Deflate = 'eJxdUUtOQzEMvMoc4OndgT0gJCT2buJWlpI4jePeqZfpmX' +
                              'AKLRKbLOzx/HK73q6vOrhCunlF1qIDJhNUeW5I2ozT5OkD' +
                              'lKWLJWkncJG5403HQXAkT3Jw29B9uIEmToMukglZ0vS6oc' +
                              'iBh4JG8sV4oVLEUCitK2kxq1WzPnChHDzsaGKy491LofoA' +
                              'bWh8do43oeuYhB5EPCjcLjzYJo48KrfQBvnJecNFJvHT1+' +
                              'RSQsGoC7dn2t/xjhduTA1NWyQIZR0pbHwMDatnD+crPqKS' +
                              'qGPHp1vnlsWM/07ubf7bheF7kqSj84Bm0R1fYTfaK8vqqq' +
                              'fKBtNMhe3OZh6N95CTvMX5HJJi4xOVzCgUOIMSLH7wmeOH' +
                              'aFE4RdpnGavKtrB5xzfO/Ll9';
const expectedBase64Gzip = 'H4sIAAAAAAAAA11RS05DMQy8yhzg6d2BPSAkJPZu4laWkjiN4' +
                           '96pl+mZcAotEpss7PH8crverq86uEK6eUXWogMmE1R5bkjajN' +
                           'Pk6QOUpYslaSdwkbnjTcdBcCRPcnDb0H24gSZOgy6SCVnS9Lq' +
                           'hyIGHgkbyxXihUsRQKK0raTGrVbM+cKEcPOxoYrLj3Uuh+gBt' +
                           'aHx2jjeh65iEHkQ8KNwuPNgmjjwqt9AG+cl5w0Um8dPX5FJCw' +
                           'agLt2fa3/GOF25MDU1bJAhlHSlsfAwNq2cP5ys+opKoY8enW+' +
                           'eWxYz/Tu5t/tuF4XuSpKPzgGbRHV9hN9ory+qqp8oG00yF7c5' +
                           'mHo33kJO8xfkckmLjE5XMKBQ4gxIsfvCZ44doUThF2mcZq8q2' +
                           'sHnHNzRtagj5AQAA';

zlib.deflate(inputString, function(err, buffer) {
  assert.equal(buffer.toString('base64'), expectedBase64Deflate,
               'deflate encoded string should match');
});

zlib.gzip(inputString, function(err, buffer) {
  // Can't actually guarantee that we'll get exactly the same
  // deflated bytes when we compress a string, since the header
  // depends on stuff other than the input string itself.
  // However, decrypting it should definitely yield the same
  // result that we're expecting, and this should match what we get
  // from inflating the known valid deflate data.
  zlib.gunzip(buffer, function(err, gunzipped) {
    assert.equal(gunzipped.toString(), inputString,
                 'Should get original string after gzip/gunzip');
  });
});

var buffer = Buffer.from(expectedBase64Deflate, 'base64');
zlib.unzip(buffer, function(err, buffer) {
  assert.equal(buffer.toString(), inputString,
               'decoded inflated string should match');
});

buffer = Buffer.from(expectedBase64Gzip, 'base64');
zlib.unzip(buffer, function(err, buffer) {
  assert.equal(buffer.toString(), inputString,
               'decoded gunzipped string should match');
});
