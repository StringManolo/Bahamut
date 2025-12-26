#!/usr/bin/env node

// Name: Subdomain Finder by Certificates
// Description: Finds subdomains using SSL certificate transparency logs (crt.sh)
// Type: processor
// Stage: 2
// Consumes: domain
// Provides: subdomain
// Storage: delete
// Install: npm i node-fetch@2
// InstallScope: shared
// RateLimit: 50
// Timeout: 30000
// Args: -d, --domain <domain> (optional) Target domain for standalone mode
// Args: --debug (flag, optional) Enable debug mode with ETA calculations

const pLimit = require('p-limit');

const VERSION = '1.0.0';
const USER_AGENT = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36';
const limit = pLimit(50);

const parseArgs = () => {
  const args = process.argv.slice(2);
  const parsed = { domain: null, debug: false };
  
  for (let i = 0; i < args.length; i++) {
    if ((args[i] === '-d' || args[i] === '--domain') && i + 1 < args.length) {
      parsed.domain = args[i + 1];
      i++;
    } else if (args[i] === '--debug') {
      parsed.debug = true;
    }
  }
  
  return parsed;
};

const sleep = (ms) => new Promise(resolve => setTimeout(resolve, ms));

const getRootDomain = (domain) => {
  const parts = domain.split('.');
  if (parts.length > 2) {
    return parts.slice(-2).join('.');
  }
  return domain;
};

const emitProgress = (processedDomains, totalDomains, processedRoots, totalRoots, debug = false, startTime = null) => {
  const progressMsg = { t: 'progress', c: processedDomains, T: totalDomains };
  let message = `Processed ${processedDomains}/${totalDomains} domains (${processedRoots}/${totalRoots} root queries)`;
  
  if (debug && startTime && processedRoots > 0) {
    const elapsedSeconds = (Date.now() - startTime) / 1000;
    const rootsRemaining = totalRoots - processedRoots;
    const rootsPerSecond = processedRoots / elapsedSeconds;
    const etaSeconds = rootsRemaining / rootsPerSecond;
    const etaMinutes = Math.max(0, Math.round(etaSeconds / 60));
    const percentDomains = Math.round((processedDomains / totalDomains) * 100);
    const percentRoots = Math.round((processedRoots / totalRoots) * 100);
    message += ` - ETA: ${etaMinutes} min (${rootsPerSecond.toFixed(2)} root domains/sec, ${percentDomains}% domains, ${percentRoots}% queries)`;
  }
  
  progressMsg.m = message;
  console.error(JSON.stringify(progressMsg));
};

const emitBatch = (domains) => {
  if (domains.length === 0) return;
  console.log(JSON.stringify({
    t: 'batch',
    f: 'domain',
    c: domains.length
  }));
  domains.forEach(domain => process.stdout.write(domain + '\n'));
  console.log(JSON.stringify({ t: 'batch_end' }));
};

const main = async () => {
  // Cargar node-fetch dinámicamente (compatible con ESM)
  let fetch;
  try {
    fetch = (await import('node-fetch')).default;
  } catch (e) {
    console.error(JSON.stringify({
      t: 'log',
      l: 'error',
      m: `Failed to load node-fetch: ${e.message}. Ensure node-fetch v3+ is installed.`
    }));
    console.log(JSON.stringify({t: 'result', ok: false, error: 'Missing node-fetch'}));
    process.exit(1);
  }

  const args = parseArgs();

  console.log(JSON.stringify({
    bmop: '1.0',
    module: 'subdomain-finder-by-certificates',
    version: VERSION,
    pid: process.pid
  }));

  const fetchWithRetry = async (url, maxRetries = 2) => {
    let lastError;
    for (let attempt = 1; attempt <= maxRetries; attempt++) {
      try {
        const response = await fetch(url, {
          headers: { 'User-Agent': USER_AGENT, 'Accept': 'application/json' },
          timeout: 8000
        });
        if (response.ok) return response;
        lastError = new Error(`HTTP ${response.status}`);
        if (attempt < maxRetries) {
          await sleep(500 * attempt);
        }
      } catch (error) {
        lastError = error;
        if (attempt < maxRetries) {
          await sleep(500 * attempt);
        }
      }
    }
    throw lastError;
  };

  const getSubdomainsFromCrt = async (rootDomain) => {
    const cleanDomain = rootDomain.toLowerCase().trim();
    const queryDomain = cleanDomain.startsWith('.') ? cleanDomain : `.${cleanDomain}`;
    const url = `https://crt.sh/?q=${encodeURIComponent(queryDomain)}&output=json`;
    
    try {
      const response = await fetchWithRetry(url);
      if (!response.ok) return [];
      const data = await response.json();
      if (!Array.isArray(data)) return [];
      
      const domains = new Set();
      
      for (const entry of data) {
        if (entry.name_value) {
          const names = typeof entry.name_value === 'string' 
            ? entry.name_value.split('\n') 
            : [entry.name_value];
          for (const name of names) {
            const cleanName = name.trim().toLowerCase();
            if (cleanName && (cleanName === cleanDomain || cleanName.endsWith('.' + cleanDomain))) {
              domains.add(cleanName);
            }
          }
        }
      }
      return Array.from(domains);
    } catch (error) {
      console.error(JSON.stringify({
        t: 'log',
        l: 'error',
        m: `Failed to fetch crt.sh for ${cleanDomain}: ${error.message}`
      }));
      return [];
    }
  };

  let processedCount = 0;
  let foundCount = 0;
  let errorCount = 0;

  const processSingleDomain = async (domain) => {
    return limit(async () => {
      processedCount++;
      const subdomains = await getSubdomainsFromCrt(domain);
      const allDomains = new Set(subdomains);
      allDomains.add(domain);
      if (allDomains.size > 0) {
        foundCount += allDomains.size;
        emitBatch(Array.from(allDomains).sort());
      }
    });
  };

  const processBMOPInput = async () => {
    let inputBuffer = '';
    const domainGroups = {};
    let totalDomains = 0;

    process.stdin.on('data', (chunk) => {
      inputBuffer += chunk.toString();
      const lines = inputBuffer.split('\n');
      inputBuffer = lines.pop();

      for (const line of lines) {
        const trimmed = line.trim();
        if (!trimmed) continue;

        try {
          const msg = JSON.parse(trimmed);
          if (msg.t === 'd' && msg.f === 'domain') {
            const domain = msg.v.trim();
            if (domain && domain.includes('.') && !domain.startsWith('*')) {
              const rootDomain = getRootDomain(domain);
              if (!domainGroups[rootDomain]) {
                domainGroups[rootDomain] = [];
              }
              domainGroups[rootDomain].push(domain);
              totalDomains++;
            }
          }
        } catch (e) {
          errorCount++;
        }
      }
    });

    return new Promise((resolve) => {
      process.stdin.on('end', async () => {
        const rootDomains = Object.keys(domainGroups);
        const totalRootDomains = rootDomains.length;
        console.error(JSON.stringify({
          t: 'log',
          l: 'info',
          m: `Aggregated ${totalDomains} domains into ${totalRootDomains} root domains (avg ${(totalDomains/totalRootDomains).toFixed(1)} domains/query)`
        }));

        const startTime = args.debug ? Date.now() : null;
        let processedDomains = 0;
        let processedRootDomains = 0;
        let lastProgressTime = 0;
        const progressInterval = 1000;

        emitProgress(0, totalDomains, 0, totalRootDomains, args.debug, startTime);

        for (const rootDomain of rootDomains) {
          const domainsInGroup = domainGroups[rootDomain];
          let allSubdomainsFromCrt;
          
          try {
            allSubdomainsFromCrt = await getSubdomainsFromCrt(rootDomain);
          } catch (err) {
            allSubdomainsFromCrt = [];
            errorCount++;
            if (args.debug) {
              console.error(JSON.stringify({
                t: 'log',
                l: 'error',
                m: `Error fetching crt.sh for ${rootDomain}: ${err.message}`
              }));
            }
          }

          const batchResults = new Set();

          for (const domain of domainsInGroup) {
            batchResults.add(domain);
            const matching = allSubdomainsFromCrt.filter(sd => 
              sd === domain || sd.endsWith('.' + domain)
            );
            matching.forEach(m => batchResults.add(m));
            processedDomains++;
            processedCount++;
          }

          processedRootDomains++;

          if (batchResults.size > 0) {
            foundCount += batchResults.size;
            emitBatch(Array.from(batchResults).sort());
          }

          const now = Date.now();
          if (now - lastProgressTime > progressInterval || processedRootDomains === totalRootDomains) {
            emitProgress(processedDomains, totalDomains, processedRootDomains, totalRootDomains, args.debug, startTime);
            lastProgressTime = now;
          }
        }

        resolve();
      });
    });
  };

  if (args.domain) {
    console.error(JSON.stringify({
      t: 'log',
      l: 'info',
      m: `Processing domain: ${args.domain}`
    }));
    await processSingleDomain(args.domain);
  } else {
    console.error(JSON.stringify({
      t: 'log',
      l: 'info',
      m: 'Starting in BMOP mode, reading domains from stdin'
    }));
    await processBMOPInput();
  }

  console.error(JSON.stringify({
    t: 'log',
    l: 'info',
    m: `Processing complete: ${processedCount} domains processed, ${foundCount} subdomains found, ${errorCount} errors`
  }));
  console.log(JSON.stringify({
    t: 'result',
    ok: true,
    count: foundCount,
    stats: { processed: processedCount, found: foundCount, errors: errorCount }
  }));
};

main().catch(err => {
  console.error(JSON.stringify({
    t: 'log',
    l: 'error',
    m: `Module execution failed: ${err.message}`
  }));
  console.log(JSON.stringify({
    t: 'result',
    ok: false,
    error: err.message
  }));
  process.exit(1);
});
