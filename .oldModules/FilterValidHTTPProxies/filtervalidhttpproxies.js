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

const axios = require('axios');

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
let currentBatch = 0;
let totalBatches = 0;
let batchStartTime = 0;
let totalProcessed = 0;

const testProxy = async (proxy) => {
  const [ip, port] = proxy.split(':');
  
  try {
    const response = await axios.get('http://example.com', {
      proxy: {
        protocol: 'http',
        host: ip,
        port: parseInt(port)
      },
      timeout: 5000,
      headers: {
        'User-Agent': 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36'
      }
    });
    
    if (response.status === 200 && response.data.includes('<title>Example Domain</title>')) {
      return { proxy, status: 'working' };
    } else {
      return { proxy, status: 'dead' };
    }
  } catch (err) {
    return { proxy, status: 'dead' };
  }
};

const processBatch = async (proxies) => {
  const batchSize = proxies.length;
  
  // Actualizar totalBatches si es necesario
  currentBatch++;
  if (currentBatch > totalBatches) {
    totalBatches = currentBatch;
  }
  
  batchStartTime = Date.now();
  
  // Solo mostrar inicio del batch cada 10 batches para reducir spam
  if (currentBatch % 10 === 0 || currentBatch <= 5) {
    console.error(JSON.stringify({
      t: "log",
      l: "info",
      m: `Starting batch ${currentBatch} (${batchSize} proxies)`
    }));
  }

  const tasks = proxies.map(proxy => () => testProxy(proxy));
  const results = await limitConcurrency(tasks, 50);

  // Filtrar proxies funcionales
  const workingProxies = results
    .filter(r => r.status === 'working')
    .map(r => r.proxy);

  // Actualizar contadores
  const batchWorking = workingProxies.length;
  const batchDead = results.length - batchWorking;
  workingCount += batchWorking;
  deadCount += batchDead;
  totalProcessed += results.length;
  
  // Calcular estadísticas
  const batchTime = Date.now() - batchStartTime;
  const batchSpeed = batchTime > 0 ? Math.round(batchSize / (batchTime / 1000)) : 0;
  const overallProgress = Math.round((totalProcessed / inputCount) * 100);
  
  // Mostrar progreso cada batch (pero no demasiado spam)
  if (currentBatch % 5 === 0 || batchTime > 30000) {
    console.error(JSON.stringify({
      t: "log",
      l: "info",
      m: `Batch ${currentBatch}: ${batchWorking} working, ${batchDead} dead (${batchTime}ms, ${batchSpeed}/sec) - Overall: ${totalProcessed}/${inputCount} (${overallProgress}%)`
    }));
  }

  // Emitir proxies funcionales
  if (workingProxies.length > 0) {
    console.log(JSON.stringify({t:"batch",f:"httpproxy",c:workingProxies.length}));
    workingProxies.forEach(proxy => {
      process.stdout.write(proxy + '\n');
    });
    console.log(JSON.stringify({t:"batch_end"}));
  }

  return workingProxies;
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
          
          if (processingQueue.length >= 500) {
            const batch = processingQueue.splice(0, 500);
            processBatch(batch).catch(err => {
              console.error(JSON.stringify({
                t: "log",
                l: "error",
                m: `Batch processing error: ${err.message}`
              }));
            });
          }
        }
      } else if (msg.t === 'batch_end') {
        // Ignorar
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
    m: `Input complete: ${inputCount} total proxies, ${processingQueue.length} remaining`
  }));

  if (processingQueue.length > 0) {
    console.error(JSON.stringify({
      t: "log",
      l: "info",
      m: `Processing final batch (${processingQueue.length} proxies)`
    }));
    
    await processBatch(processingQueue);
    processingQueue = [];
  }

  // Calcular estadísticas finales
  const successRate = inputCount > 0 ? Math.round((workingCount / inputCount) * 100) : 0;
  
  console.error(JSON.stringify({
    t: "log",
    l: "info",
    m: `Validation completed: ${workingCount} working, ${deadCount} dead (${successRate}% success rate)`
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
      successRate: successRate
    }
  }));
});

process.on('SIGINT', () => {
  console.error(JSON.stringify({
    t: "log",
    l: "info",
    m: "Process interrupted by user"
  }));
  
  console.error(JSON.stringify({
    t: "log",
    l: "info",
    m: `Progress: ${totalProcessed}/${inputCount} processed, ${workingCount} working, ${deadCount} dead`
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
