#!/usr/bin/env node

// Name: Proxy Validator
// Description: Validates HTTP proxies by testing them against example.com and checking for expected title
// Type: processor
// Stage: 2
// Consumes: httpproxy
// Provides: httpproxy
// Storage: replace
// InstallScope: shared
// Install: npm install axios
// RateLimit: 50
// Timeout: 5000
// Args: --secure (flag, optional) Enable secure mode (HTTPS) with port adjustment

const axios = require('axios');

// Parse command line arguments
const args = process.argv.slice(2);
const secureMode = args.includes('--secure');

console.log(JSON.stringify({bmop:"1.0",module:"proxy-validator",pid:process.pid}));

const limitConcurrency = (tasks, concurrency) => {
  return new Promise((resolve, reject) => {
    const results = [];
    let currentIndex = 0;
    let running = 0;
    let rejected = false;

    const runNext = () => {
      if (rejected) return;

      while (running < concurrency && currentIndex < tasks.length) {
        const taskIndex = currentIndex++;
        const task = tasks[taskIndex];
        running++;

        Promise.resolve()
          .then(() => task())
          .then(result => {
            results[taskIndex] = result;
            running--;
            runNext();
          })
          .catch(error => {
            results[taskIndex] = error;
            running--;
            runNext();
          });
      }

      if (running === 0 && currentIndex === tasks.length) {
        resolve(results);
      }
    };

    runNext();
  });
};

let buffer = '';
let inputCount = 0;
let workingCount = 0;
let deadCount = 0;
let jsonErrors = 0;
let processingQueue = [];

// Variables para el progreso
let totalBatches = 0;
let globalStartTime = Date.now();
let completedBatches = 0;

const testProxy = async (proxy) => {
  const [ip, originalPort] = proxy.split(':');
  const port = parseInt(originalPort);

  // Determine ports to test based on secure mode
  const portsToTest = [];
  if (!secureMode) {
    // Original behavior: test only the original port with HTTP
    portsToTest.push(port);
  } else {
    // Secure mode: adjust ports for HTTPS
    if (port === 80) {
      portsToTest.push(443); // HTTP default -> HTTPS default
    } else if (port === 8000) {
      portsToTest.push(8443); // Common HTTP alt -> HTTPS alt
    } else {
      portsToTest.push(port); // Try original port first
      if (port !== 443) {
        portsToTest.push(443); // Then try default HTTPS port
      }
    }
  }

  const url = secureMode ? 'https://example.com' : 'http://example.com';

  // Try each port in order
  for (const testPort of portsToTest) {
    try {
      const response = await axios.get(url, {
        proxy: {
          protocol: 'http',
          host: ip,
          port: testPort
        },
        timeout: 5000,
        headers: {
          'User-Agent': 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36'
        }
      });

      if (response.status === 200 && response.data.includes('<title>Example Domain</title>')) {
        // If we used a different port than original, return proxy with adjusted port
        const workingProxy = testPort === port ? proxy : `${ip}:${testPort}`;
        return { proxy: workingProxy, status: 'working' };
      }
    } catch (err) {
      // This port failed, try the next one
      continue;
    }
  }

  // All ports failed
  return { proxy, status: 'dead' };
};

const processBatch = async (proxies, batchNumber) => {
  const batchSize = proxies.length;
  const batchStartTime = Date.now();

  console.error(JSON.stringify({
    t: "log",
    l: "info",
    m: `Processing batch ${batchNumber} (${batchSize} proxies)${secureMode ? ' in SECURE mode' : ''}`
  }));

  const tasks = proxies.map(proxy => () => testProxy(proxy));
  const results = await limitConcurrency(tasks, 50);

  const workingProxies = results
    .filter(r => r.status === 'working')
    .map(r => r.proxy);

  const batchWorking = workingProxies.length;
  const batchDead = results.length - batchWorking;

  const batchTime = Date.now() - batchStartTime;
  const batchSpeed = batchTime > 0 ? Math.round(batchSize / (batchTime / 1000)) : 0;

  console.error(JSON.stringify({
    t: "log",
    l: "info",
    m: `Batch ${batchNumber}: ${batchWorking} working, ${batchDead} dead (${batchTime}ms, ${batchSpeed}/sec)`
  }));

  if (workingProxies.length > 0) {
    console.log(JSON.stringify({t:"batch",f:"httpproxy",c:workingProxies.length}));
    workingProxies.forEach(proxy => {
      process.stdout.write(proxy + '\n');
    });
    console.log(JSON.stringify({t:"batch_end"}));
  }

  return {
    working: batchWorking,
    dead: batchDead,
    proxies: workingProxies
  };
};

process.stdin.on('data', chunk => {
  buffer += chunk.toString();
  let lines = buffer.split(/\r?\n/);
  buffer = lines.pop();

  for (const line of lines) {
    const trimmed = line.trim();
    if (!trimmed) continue;

    try {
      const msg = JSON.parse(trimmed);
      if (msg.t === 'd' && msg.f === 'httpproxy') {
        inputCount++;
        const proxy = msg.v.trim();
        if (proxy && proxy.includes(':')) {
          processingQueue.push(proxy);
        }
      } else if (msg.t === 'batch_end') {
        // No action needed
      }
    } catch (e) {
      jsonErrors++;
      console.error(JSON.stringify({
        t: "log",
        l: "warn",
        m: `JSON Parse Error: ${e.message}`
      }));
    }
  }
});

process.stdin.on('end', async () => {
  console.error(JSON.stringify({
    t: "log",
    l: "info",
    m: `Input complete: ${inputCount} total proxies, ${processingQueue.length} in queue${secureMode ? ' (SECURE mode)' : ''}`
  }));

  totalBatches = Math.ceil(processingQueue.length / 500);
  console.error(JSON.stringify({
    t: "log",
    l: "info",
    m: `Will process ${totalBatches} batches of up to 500 proxies each`
  }));

  let batchNumber = 0;

  while (processingQueue.length > 0) {
    batchNumber++;
    const batchSize = Math.min(500, processingQueue.length);
    const batch = processingQueue.splice(0, batchSize);

    const result = await processBatch(batch, batchNumber);

    workingCount += result.working;
    deadCount += result.dead;
    completedBatches++;

    if (batchNumber % 10 === 0) {
      const elapsedTime = Date.now() - globalStartTime;
      const processedSoFar = (workingCount + deadCount);
      const globalProgress = Math.round((processedSoFar / inputCount) * 100);
      const globalSpeed = elapsedTime > 0 ? Math.round(processedSoFar / (elapsedTime / 1000)) : 0;

      console.error(JSON.stringify({
        t: "log",
        l: "info",
        m: `Global: ${processedSoFar}/${inputCount} (${globalProgress}%), ${workingCount} working, ${deadCount} dead, ${globalSpeed}/sec, ${completedBatches}/${totalBatches} batches`
      }));
    }
  }

  const totalTime = Date.now() - globalStartTime;
  const successRate = inputCount > 0 ? Math.round((workingCount / inputCount) * 100) : 0;
  const avgSpeed = totalTime > 0 ? Math.round(inputCount / (totalTime / 1000)) : 0;

  console.error(JSON.stringify({
    t: "log",
    l: "info",
    m: `Validation completed in ${Math.round(totalTime/1000)}s${secureMode ? ' (SECURE mode)' : ''}`
  }));

  console.error(JSON.stringify({
    t: "log",
    l: "info",
    m: `Results: ${workingCount} working, ${deadCount} dead (${successRate}% success rate, ${avgSpeed}/sec avg)`
  }));

  console.log(JSON.stringify({
    t: "result",
    ok: true,
    count: workingCount,
    stats: {
      input: inputCount,
      working: workingCount,
      dead: deadCount,
      jsonErrors: jsonErrors,
      successRate: successRate,
      totalTime: totalTime,
      avgSpeed: avgSpeed,
      secureMode: secureMode
    }
  }));
});

process.on('SIGINT', () => {
  const elapsedTime = Date.now() - globalStartTime;
  const processedSoFar = workingCount + deadCount;

  console.error(JSON.stringify({
    t: "log",
    l: "info",
    m: "Process interrupted by user"
  }));

  console.error(JSON.stringify({
    t: "log",
    l: "info",
    m: `Progress: ${processedSoFar}/${inputCount} processed, ${workingCount} working, ${deadCount} dead in ${Math.round(elapsedTime/1000)}s${secureMode ? ' (SECURE mode)' : ''}`
  }));

  process.exit(0);
});

process.on('uncaughtException', (err) => {
  console.error(JSON.stringify({
    t: "log",
    l: "error",
    m: `Uncaught exception: ${err.message}`
  }));

  process.exit(1);
});
