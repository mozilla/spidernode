'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

const DH_NOT_SUITABLE_GENERATOR = crypto.constants.DH_NOT_SUITABLE_GENERATOR;

// Test Diffie-Hellman with two parties sharing a secret,
// using various encodings as we go along
const dh1 = crypto.createDiffieHellman(common.hasFipsCrypto ? 1024 : 256);
const p1 = dh1.getPrime('buffer');
const dh2 = crypto.createDiffieHellman(p1, 'buffer');
let key1 = dh1.generateKeys();
let key2 = dh2.generateKeys('hex');
let secret1 = dh1.computeSecret(key2, 'hex', 'base64');
let secret2 = dh2.computeSecret(key1, 'latin1', 'buffer');

assert.strictEqual(secret2.toString('base64'), secret1);
assert.strictEqual(dh1.verifyError, 0);
assert.strictEqual(dh2.verifyError, 0);

const argumentsError =
  /^TypeError: First argument should be number, string, Buffer, TypedArray, or DataView$/;

assert.throws(() => {
  crypto.createDiffieHellman([0x1, 0x2]);
}, argumentsError);

assert.throws(() => {
  crypto.createDiffieHellman(() => { });
}, argumentsError);

assert.throws(() => {
  crypto.createDiffieHellman(/abc/);
}, argumentsError);

assert.throws(() => {
  crypto.createDiffieHellman({});
}, argumentsError);

// Create "another dh1" using generated keys from dh1,
// and compute secret again
const dh3 = crypto.createDiffieHellman(p1, 'buffer');
const privkey1 = dh1.getPrivateKey();
dh3.setPublicKey(key1);
dh3.setPrivateKey(privkey1);

assert.deepStrictEqual(dh1.getPrime(), dh3.getPrime());
assert.deepStrictEqual(dh1.getGenerator(), dh3.getGenerator());
assert.deepStrictEqual(dh1.getPublicKey(), dh3.getPublicKey());
assert.deepStrictEqual(dh1.getPrivateKey(), dh3.getPrivateKey());
assert.strictEqual(dh3.verifyError, 0);

const secret3 = dh3.computeSecret(key2, 'hex', 'base64');

assert.strictEqual(secret1, secret3);

const wrongBlockLength =
  /^Error: error:0606506D:digital envelope routines:EVP_DecryptFinal_ex:wrong final block length$/;

// Run this one twice to make sure that the dh3 clears its error properly
{
  const c = crypto.createDecipheriv('aes-128-ecb', crypto.randomBytes(16), '');
  assert.throws(() => {
    c.final('utf8');
  }, wrongBlockLength);
}

{
  const c = crypto.createDecipheriv('aes-128-ecb', crypto.randomBytes(16), '');
  assert.throws(() => {
    c.final('utf8');
  }, wrongBlockLength);
}

assert.throws(() => {
  dh3.computeSecret('');
}, /^Error: Supplied key is too small$/);

// Create a shared using a DH group.
const alice = crypto.createDiffieHellmanGroup('modp5');
const bob = crypto.createDiffieHellmanGroup('modp5');
alice.generateKeys();
bob.generateKeys();
const aSecret = alice.computeSecret(bob.getPublicKey()).toString('hex');
const bSecret = bob.computeSecret(alice.getPublicKey()).toString('hex');
assert.strictEqual(aSecret, bSecret);
assert.strictEqual(alice.verifyError, DH_NOT_SUITABLE_GENERATOR);
assert.strictEqual(bob.verifyError, DH_NOT_SUITABLE_GENERATOR);

/* Ensure specific generator (buffer) works as expected.
 * The values below (modp2/modp2buf) are for a 1024 bits long prime from
 * RFC 2412 E.2, see https://tools.ietf.org/html/rfc2412. */
const modp2 = crypto.createDiffieHellmanGroup('modp2');
const modp2buf = Buffer.from([
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc9, 0x0f,
  0xda, 0xa2, 0x21, 0x68, 0xc2, 0x34, 0xc4, 0xc6, 0x62, 0x8b,
  0x80, 0xdc, 0x1c, 0xd1, 0x29, 0x02, 0x4e, 0x08, 0x8a, 0x67,
  0xcc, 0x74, 0x02, 0x0b, 0xbe, 0xa6, 0x3b, 0x13, 0x9b, 0x22,
  0x51, 0x4a, 0x08, 0x79, 0x8e, 0x34, 0x04, 0xdd, 0xef, 0x95,
  0x19, 0xb3, 0xcd, 0x3a, 0x43, 0x1b, 0x30, 0x2b, 0x0a, 0x6d,
  0xf2, 0x5f, 0x14, 0x37, 0x4f, 0xe1, 0x35, 0x6d, 0x6d, 0x51,
  0xc2, 0x45, 0xe4, 0x85, 0xb5, 0x76, 0x62, 0x5e, 0x7e, 0xc6,
  0xf4, 0x4c, 0x42, 0xe9, 0xa6, 0x37, 0xed, 0x6b, 0x0b, 0xff,
  0x5c, 0xb6, 0xf4, 0x06, 0xb7, 0xed, 0xee, 0x38, 0x6b, 0xfb,
  0x5a, 0x89, 0x9f, 0xa5, 0xae, 0x9f, 0x24, 0x11, 0x7c, 0x4b,
  0x1f, 0xe6, 0x49, 0x28, 0x66, 0x51, 0xec, 0xe6, 0x53, 0x81,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
]);

{
  const exmodp2 = crypto.createDiffieHellman(modp2buf, Buffer.from([2]));
  modp2.generateKeys();
  exmodp2.generateKeys();
  const modp2Secret = modp2.computeSecret(exmodp2.getPublicKey())
      .toString('hex');
  const exmodp2Secret = exmodp2.computeSecret(modp2.getPublicKey())
      .toString('hex');
  assert.strictEqual(modp2Secret, exmodp2Secret);
  assert.strictEqual(modp2.verifyError, DH_NOT_SUITABLE_GENERATOR);
  assert.strictEqual(exmodp2.verifyError, DH_NOT_SUITABLE_GENERATOR);
}

for (const buf of [modp2buf, ...common.getArrayBufferViews(modp2buf)]) {
  // Ensure specific generator (string with encoding) works as expected with
  // any ArrayBufferViews as the first argument to createDiffieHellman().
  const exmodp2 = crypto.createDiffieHellman(buf, '02', 'hex');
  exmodp2.generateKeys();
  const modp2Secret = modp2.computeSecret(exmodp2.getPublicKey())
      .toString('hex');
  const exmodp2Secret = exmodp2.computeSecret(modp2.getPublicKey())
      .toString('hex');
  assert.strictEqual(modp2Secret, exmodp2Secret);
  assert.strictEqual(exmodp2.verifyError, DH_NOT_SUITABLE_GENERATOR);
}

{
  // Ensure specific generator (string without encoding) works as expected.
  const exmodp2 = crypto.createDiffieHellman(modp2buf, '\x02');
  exmodp2.generateKeys();
  const modp2Secret = modp2.computeSecret(exmodp2.getPublicKey())
      .toString('hex');
  const exmodp2Secret = exmodp2.computeSecret(modp2.getPublicKey())
      .toString('hex');
  assert.strictEqual(modp2Secret, exmodp2Secret);
  assert.strictEqual(exmodp2.verifyError, DH_NOT_SUITABLE_GENERATOR);
}

{
  // Ensure specific generator (numeric) works as expected.
  const exmodp2 = crypto.createDiffieHellman(modp2buf, 2);
  exmodp2.generateKeys();
  const modp2Secret = modp2.computeSecret(exmodp2.getPublicKey())
      .toString('hex');
  const exmodp2Secret = exmodp2.computeSecret(modp2.getPublicKey())
      .toString('hex');
  assert.strictEqual(modp2Secret, exmodp2Secret);
  assert.strictEqual(exmodp2.verifyError, DH_NOT_SUITABLE_GENERATOR);
}


const p = 'FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74' +
          '020BBEA63B139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F1437' +
          '4FE1356D6D51C245E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED' +
          'EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE65381FFFFFFFFFFFFFFFF';
const bad_dh = crypto.createDiffieHellman(p, 'hex');
assert.strictEqual(bad_dh.verifyError, DH_NOT_SUITABLE_GENERATOR);


const availableCurves = new Set(crypto.getCurves());
const availableHashes = new Set(crypto.getHashes());

// Oakley curves do not clean up ERR stack, it was causing unexpected failure
// when accessing other OpenSSL APIs afterwards.
if (availableCurves.has('Oakley-EC2N-3')) {
  crypto.createECDH('Oakley-EC2N-3');
  crypto.createHash('sha256');
}

// Test ECDH
if (availableCurves.has('prime256v1') && availableCurves.has('secp256k1')) {
  const ecdh1 = crypto.createECDH('prime256v1');
  const ecdh2 = crypto.createECDH('prime256v1');
  key1 = ecdh1.generateKeys();
  key2 = ecdh2.generateKeys('hex');
  secret1 = ecdh1.computeSecret(key2, 'hex', 'base64');
  secret2 = ecdh2.computeSecret(key1, 'latin1', 'buffer');

  assert.strictEqual(secret1, secret2.toString('base64'));

  // Point formats
  assert.strictEqual(ecdh1.getPublicKey('buffer', 'uncompressed')[0], 4);
  let firstByte = ecdh1.getPublicKey('buffer', 'compressed')[0];
  assert(firstByte === 2 || firstByte === 3);
  firstByte = ecdh1.getPublicKey('buffer', 'hybrid')[0];
  assert(firstByte === 6 || firstByte === 7);
  // format value should be string
  assert.throws(() => {
    ecdh1.getPublicKey('buffer', 10);
  }, /^TypeError: Bad format: 10$/);

  // ECDH should check that point is on curve
  const ecdh3 = crypto.createECDH('secp256k1');
  const key3 = ecdh3.generateKeys();

  assert.throws(() => {
    ecdh2.computeSecret(key3, 'latin1', 'buffer');
  }, /^Error: Failed to translate Buffer to a EC_POINT$/);

  // ECDH should allow .setPrivateKey()/.setPublicKey()
  const ecdh4 = crypto.createECDH('prime256v1');

  ecdh4.setPrivateKey(ecdh1.getPrivateKey());
  ecdh4.setPublicKey(ecdh1.getPublicKey());

  assert.throws(() => {
    ecdh4.setPublicKey(ecdh3.getPublicKey());
  }, /^Error: Failed to convert Buffer to EC_POINT$/);

  // Verify that we can use ECDH without having to use newly generated keys.
  const ecdh5 = crypto.createECDH('secp256k1');

  // Verify errors are thrown when retrieving keys from an uninitialized object.
  assert.throws(() => {
    ecdh5.getPublicKey();
  }, /^Error: Failed to get ECDH public key$/);

  assert.throws(() => {
    ecdh5.getPrivateKey();
  }, /^Error: Failed to get ECDH private key$/);

  // A valid private key for the secp256k1 curve.
  const cafebabeKey = 'cafebabe'.repeat(8);
  // Associated compressed and uncompressed public keys (points).
  const cafebabePubPtComp =
  '03672a31bfc59d3f04548ec9b7daeeba2f61814e8ccc40448045007f5479f693a3';
  const cafebabePubPtUnComp =
  '04672a31bfc59d3f04548ec9b7daeeba2f61814e8ccc40448045007f5479f693a3' +
  '2e02c7f93d13dc2732b760ca377a5897b9dd41a1c1b29dc0442fdce6d0a04d1d';
  ecdh5.setPrivateKey(cafebabeKey, 'hex');
  assert.strictEqual(ecdh5.getPrivateKey('hex'), cafebabeKey);
  // Show that the public point (key) is generated while setting the
  // private key.
  assert.strictEqual(ecdh5.getPublicKey('hex'), cafebabePubPtUnComp);

  // Compressed and uncompressed public points/keys for other party's
  // private key.
  // 0xDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEFDEADBEEF
  const peerPubPtComp =
  '02c6b754b20826eb925e052ee2c25285b162b51fdca732bcf67e39d647fb6830ae';
  const peerPubPtUnComp =
  '04c6b754b20826eb925e052ee2c25285b162b51fdca732bcf67e39d647fb6830ae' +
  'b651944a574a362082a77e3f2b5d9223eb54d7f2f76846522bf75f3bedb8178e';

  const sharedSecret =
  '1da220b5329bbe8bfd19ceef5a5898593f411a6f12ea40f2a8eead9a5cf59970';

  assert.strictEqual(ecdh5.computeSecret(peerPubPtComp, 'hex', 'hex'),
                     sharedSecret);
  assert.strictEqual(ecdh5.computeSecret(peerPubPtUnComp, 'hex', 'hex'),
                     sharedSecret);

  // Verify that we still have the same key pair as before the computation.
  assert.strictEqual(ecdh5.getPrivateKey('hex'), cafebabeKey);
  assert.strictEqual(ecdh5.getPublicKey('hex'), cafebabePubPtUnComp);

  // Verify setting and getting compressed and non-compressed serializations.
  ecdh5.setPublicKey(cafebabePubPtComp, 'hex');
  assert.strictEqual(ecdh5.getPublicKey('hex'), cafebabePubPtUnComp);
  assert.strictEqual(
    ecdh5.getPublicKey('hex', 'compressed'),
    cafebabePubPtComp
  );
  ecdh5.setPublicKey(cafebabePubPtUnComp, 'hex');
  assert.strictEqual(ecdh5.getPublicKey('hex'), cafebabePubPtUnComp);
  assert.strictEqual(
    ecdh5.getPublicKey('hex', 'compressed'),
    cafebabePubPtComp
  );

  // Show why allowing the public key to be set on this type
  // does not make sense.
  ecdh5.setPublicKey(peerPubPtComp, 'hex');
  assert.strictEqual(ecdh5.getPublicKey('hex'), peerPubPtUnComp);
  assert.throws(() => {
    // Error because the public key does not match the private key anymore.
    ecdh5.computeSecret(peerPubPtComp, 'hex', 'hex');
  }, /^Error: Invalid key pair$/);

  // Set to a valid key to show that later attempts to set an invalid key are
  // rejected.
  ecdh5.setPrivateKey(cafebabeKey, 'hex');

  // Some invalid private keys for the secp256k1 curve.
  const errMessage = /^Error: Private key is not valid for specified curve\.$/;
  ['0000000000000000000000000000000000000000000000000000000000000000',
   'FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141',
   'FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF',
  ].forEach((element) => {
    assert.throws(() => {
      ecdh5.setPrivateKey(element, 'hex');
    }, errMessage);
    // Verify object state did not change.
    assert.strictEqual(ecdh5.getPrivateKey('hex'), cafebabeKey);
  });
}

// Use of invalid keys was not cleaning up ERR stack, and was causing
// unexpected failure in subsequent signing operations.
if (availableCurves.has('prime256v1') && availableHashes.has('sha256')) {
  const curve = crypto.createECDH('prime256v1');
  const invalidKey = Buffer.alloc(65);
  invalidKey.fill('\0');
  curve.generateKeys();
  assert.throws(() => {
    curve.computeSecret(invalidKey);
  }, /^Error: Failed to translate Buffer to a EC_POINT$/);
  // Check that signing operations are not impacted by the above error.
  const ecPrivateKey =
    '-----BEGIN EC PRIVATE KEY-----\n' +
    'MHcCAQEEIF+jnWY1D5kbVYDNvxxo/Y+ku2uJPDwS0r/VuPZQrjjVoAoGCCqGSM49\n' +
    'AwEHoUQDQgAEurOxfSxmqIRYzJVagdZfMMSjRNNhB8i3mXyIMq704m2m52FdfKZ2\n' +
    'pQhByd5eyj3lgZ7m7jbchtdgyOF8Io/1ng==\n' +
    '-----END EC PRIVATE KEY-----';
  assert.doesNotThrow(() => {
    crypto.createSign('SHA256').sign(ecPrivateKey);
  });
}

// invalid test: curve argument is undefined
assert.throws(() => {
  crypto.createECDH();
}, /^TypeError: "curve" argument should be a string$/);
