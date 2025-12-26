#!/usr/bin/env node

// Name: DNS Validator
// Description: Validates domains by DNS resolution, filtering out dead domains
// Type: processor
// Stage: 2
// Consumes: domain
// Provides: domain
// Storage: replace
// InstallScope: shared
// RateLimit: 200
// Timeout: 10000

const { Resolver } = require('dns').promises;
const pLimit = require('p-limit');

const resolver = new Resolver();
resolver.setServers(['1.1.1.1', '8.8.8.8']);

const limit = pLimit(200);

console.log(JSON.stringify({bmop:"1.0",module:"dns-validator",pid:process.pid}));

let buffer = '';
let inputCount = 0;
let aliveCount = 0;
let deadCount = 0;
let jsonErrors = 0;
let processingQueue = [];

const validateDomain = async (domain) => {
  try {
    await resolver.resolve(domain);
    return { domain, status: 'alive' };
  } catch (err) {
    return { domain, status: 'dead' };
  }
};

const processBatch = async (domains) => {
  const results = await Promise.all(
    domains.map(domain => limit(() => validateDomain(domain)))
  );

  const aliveDomains = results
    .filter(r => r.status === 'alive')
    .map(r => r.domain);

  // Emit alive domains
  if (aliveDomains.length > 0) {
    console.log(JSON.stringify({t:"batch",f:"domain",c:aliveDomains.length}));
    aliveDomains.forEach(domain => {
      process.stdout.write(domain + '\n');
    });
    console.log(JSON.stringify({t:"batch_end"}));
  }

  aliveCount += aliveDomains.length;
  deadCount += results.length - aliveDomains.length;

  return aliveDomains;
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
      if (msg.t === 'd' && msg.f === 'domain') {
        inputCount++;
        const domain = msg.v.trim().toLowerCase();
        
        // Basic domain validation
        if (domain && domain.length > 3 && domain.includes('.') && !domain.startsWith('*')) {
          processingQueue.push(domain);
          
          // Process in batches of 1000 to avoid memory issues
          if (processingQueue.length >= 1000) {
            const batch = processingQueue.splice(0, 1000);
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
        // Ignore batch_end messages
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
  // Process remaining domains in queue
  if (processingQueue.length > 0) {
    await processBatch(processingQueue);
    processingQueue = [];
  }

  console.error(JSON.stringify({
    t: "log",
    l: "info",
    m: `DNS validation completed: ${inputCount} input, ${aliveCount} alive, ${deadCount} dead, ${jsonErrors} JSON errors`
  }));

  console.log(JSON.stringify({
    t: "result",
    ok: true,
    count: aliveCount,
    stats: {
      input: inputCount,
      alive: aliveCount,
      dead: deadCount,
      jsonErrors: jsonErrors
    }
  }));
});

// Handle process termination
process.on('SIGINT', () => {
  console.error(JSON.stringify({
    t: "log",
    l: "info",
    m: "Process interrupted by user"
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
