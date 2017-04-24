'use strict';

const child_process = require('child_process');
const path = require('path');
const fs = require('fs');

const requirementsURL =
  'https://github.com/nodejs/node/blob/master/doc/guides/writing-and-running-benchmarks.md#http-benchmark-requirements';

// The port used by servers and wrk
exports.PORT = process.env.PORT || 12346;

class AutocannonBenchmarker {
  constructor() {
    this.name = 'autocannon';
    this.executable = process.platform === 'win32' ?
                      'autocannon.cmd' :
                      'autocannon';
    const result = child_process.spawnSync(this.executable, ['-h']);
    this.present = !(result.error && result.error.code === 'ENOENT');
  }

  create(options) {
    const args = [
      '-d', options.duration,
      '-c', options.connections,
      '-j',
      '-n',
      `http://127.0.0.1:${options.port}${options.path}`
    ];
    const child = child_process.spawn(this.executable, args);
    return child;
  }

  processResults(output) {
    let result;
    try {
      result = JSON.parse(output);
    } catch (err) {
      return undefined;
    }
    if (!result || !result.requests || !result.requests.average) {
      return undefined;
    } else {
      return result.requests.average;
    }
  }
}

class WrkBenchmarker {
  constructor() {
    this.name = 'wrk';
    this.executable = 'wrk';
    const result = child_process.spawnSync(this.executable, ['-h']);
    this.present = !(result.error && result.error.code === 'ENOENT');
  }

  create(options) {
    const args = [
      '-d', options.duration,
      '-c', options.connections,
      '-t', 8,
      `http://127.0.0.1:${options.port}${options.path}`
    ];
    const child = child_process.spawn(this.executable, args);
    return child;
  }

  processResults(output) {
    const throughputRe = /Requests\/sec:[ \t]+([0-9.]+)/;
    const match = output.match(throughputRe);
    const throughput = match && +match[1];
    if (!isFinite(throughput)) {
      return undefined;
    } else {
      return throughput;
    }
  }
}

/**
 * Simple, single-threaded benchmarker for testing if the benchmark
 * works
 */
class TestDoubleBenchmarker {
  constructor() {
    this.name = 'test-double';
    this.executable = path.resolve(__dirname, '_test-double-benchmarker.js');
    this.present = fs.existsSync(this.executable);
  }

  create(options) {
    const child = child_process.fork(this.executable, {
      silent: true,
      env: {
        duration: options.duration,
        connections: options.connections,
        path: `http://127.0.0.1:${options.port}${options.path}`
      }
    });
    return child;
  }

  processResults(output) {
    let result;
    try {
      result = JSON.parse(output);
    } catch (err) {
      return undefined;
    }
    return result.throughput;
  }
}

const http_benchmarkers = [
  new WrkBenchmarker(),
  new AutocannonBenchmarker(),
  new TestDoubleBenchmarker()
];

const benchmarkers = {};

http_benchmarkers.forEach((benchmarker) => {
  benchmarkers[benchmarker.name] = benchmarker;
  if (!exports.default_http_benchmarker && benchmarker.present) {
    exports.default_http_benchmarker = benchmarker.name;
  }
});

exports.run = function(options, callback) {
  options = Object.assign({
    port: exports.PORT,
    path: '/',
    connections: 100,
    duration: 10,
    benchmarker: exports.default_http_benchmarker
  }, options);
  if (!options.benchmarker) {
    callback(new Error(`Could not locate required http benchmarker. See ${
                        requirementsURL} for further instructions.`));
    return;
  }
  const benchmarker = benchmarkers[options.benchmarker];
  if (!benchmarker) {
    callback(new Error(`Requested benchmarker '${
                        options.benchmarker}' is  not supported`));
    return;
  }
  if (!benchmarker.present) {
    callback(new Error(`Requested benchmarker '${
                        options.benchmarker}' is  not installed`));
    return;
  }

  const benchmarker_start = process.hrtime();

  const child = benchmarker.create(options);

  child.stderr.pipe(process.stderr);

  let stdout = '';
  child.stdout.on('data', (chunk) => stdout += chunk.toString());

  child.once('close', function(code) {
    const elapsed = process.hrtime(benchmarker_start);
    if (code) {
      let error_message = `${options.benchmarker} failed with ${code}.`;
      if (stdout !== '') {
        error_message += ` Output: ${stdout}`;
      }
      callback(new Error(error_message), code);
      return;
    }

    const result = benchmarker.processResults(stdout);
    if (result === undefined) {
      callback(new Error(
        `${options.benchmarker} produced strange output: ${stdout}`), code);
      return;
    }

    callback(null, code, options.benchmarker, result, elapsed);
  });

};
