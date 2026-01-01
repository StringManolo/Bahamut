#!/usr/bin/env node
// Name: Subdomain Finder by Certificates
// Description: Finds subdomains using SSL certificate transparency logs (crt.sh) with TLD-aware grouping
// Type: processor
// Stage: 1
// Consumes: *
// Provides: subdomain
// Storage: delete
// InstallScope: shared
// RateLimit: 100
// Timeout: 30000
// Args: -d, --domain <domain> (optional) Target domain for standalone mode
// Args: --debug (flag, optional) Enable debug mode with ETA calculations
// Args: --use-http-proxies (flag, optional) Use HTTP proxies to avoid rate limiting
// Args: --proxy-insecure (flag, optional) Disable TLS certificate validation for proxies
// Args: --user-agent <agent> (optional) Custom User-Agent string
// Install: npm install psl p-limit

import { createRequire } from 'module';
import { execFile } from 'child_process';
import { promisify } from 'util';
import { appendFileSync, writeFileSync } from 'fs';

const require = createRequire(import.meta.url);
const execFileAsync = promisify(execFile);

const pLimit = require('p-limit').default;
const psl = require('psl');

const VERSION = '1.1.0';
const DEFAULT_USER_AGENT = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36';
const limit = pLimit(100);

const parseArgs = () => {
  const args = process.argv.slice(2);
  const parsed = { 
    domain: null, 
    debug: false, 
    useHttpProxies: false, 
    proxyInsecure: false,
    userAgent: DEFAULT_USER_AGENT 
  };

  for (let i = 0; i < args.length; i++) {
    if ((args[i] === '-d' || args[i] === '--domain') && i + 1 < args.length) {
      parsed.domain = args[i + 1];
      i++;
    } else if (args[i] === '--debug') {
      parsed.debug = true;
    } else if (args[i] === '--use-http-proxies') {
      parsed.useHttpProxies = true;
    } else if (args[i] === '--proxy-insecure') {
      parsed.proxyInsecure = true;
    } else if (args[i] === '--user-agent' && i + 1 < args.length) {
      parsed.userAgent = args[i + 1];
      i++;
    }
  }

  return parsed;
};

const sleep = (ms) => new Promise(resolve => setTimeout(resolve, ms));

const getRootDomain = (domain) => {
  try {
    const parsed = psl.parse(domain);
    return parsed.domain || null;
  } catch (e) {
    return null;
  }
};

const isTLDOrInvalid = (domain) => {
  const root = getRootDomain(domain);
  return root === null;
};

const isValidDomain = (domain) => {
  if (!domain || typeof domain !== 'string') return false;
  
  // Clean domain string
  const cleanDomain = domain
    .trim()
    .toLowerCase()
    .replace(/^(https?:\/\/)?(www\.)?/, '')
    .replace(/\/.*$/, '');
  
  // Basic domain format validation
  if (cleanDomain.length > 253) return false;
  
  const domainRegex = /^(?:(?!-)[a-zA-Z0-9-]{1,63}(?<!-)\.)+(?!-)[a-zA-Z]{2,63}(?<!-)$/;
  return domainRegex.test(cleanDomain);
};

const checkCurlInstalled = async () => {
  try {
    await execFileAsync('curl', ['--version']);
    return true;
  } catch (error) {
    throw new Error('curl is required but not installed. Please install curl.');
  }
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

const emitBatch = (subdomains) => {
  if (subdomains.length === 0) return;
  console.log(JSON.stringify({
    t: 'batch',
    f: 'subdomain',
    c: subdomains.length
  }));
  subdomains.forEach(subdomain => process.stdout.write(subdomain + '\n'));
  console.log(JSON.stringify({ t: 'batch_end' }));
};

const normalizeProxy = (proxyString) => {
  const trimmed = proxyString.trim();
  if (!trimmed) return null;

  if (trimmed.startsWith('http://') || trimmed.startsWith('https://') ||
      trimmed.startsWith('socks4://') || trimmed.startsWith('socks5://')) {
    return trimmed;
  }

  if (trimmed.includes(':') && !trimmed.startsWith('[')) {
    return `http://${trimmed}`;
  }

  return null;
};

const extractJsonFromMixedResponse = (stdout) => {
  if (!stdout || stdout.length === 0) {
    return null;
  }

  // Strategy 1: Try to find complete JSON array with regex
  const jsonRegex = /\[\s*\{[\s\S]*?\}\s*\]/g;
  const matches = stdout.match(jsonRegex);
  
  if (matches) {
    for (const match of matches) {
      try {
        const parsed = JSON.parse(match);
        if (Array.isArray(parsed) && parsed.length > 0 && parsed[0].issuer_ca_id !== undefined) {
          return parsed;
        }
      } catch (e) {
        // Continue with other strategies
      }
    }
  }

  // Strategy 2: Look for JSON starting with [{"issuer_ca_id"
  const jsonStartMarker = '[{"issuer_ca_id"';
  const startIndex = stdout.indexOf(jsonStartMarker);

  if (startIndex !== -1) {
    // Found start of JSON, now find the end of the array
    let bracketCount = 0;
    let inString = false;
    let escapeNext = false;
    let jsonEnd = -1;

    for (let i = startIndex; i < stdout.length; i++) {
      const char = stdout[i];

      if (escapeNext) {
        escapeNext = false;
        continue;
      }

      if (char === '\\') {
        escapeNext = true;
        continue;
      }

      if (char === '"' && !escapeNext) {
        inString = !inString;
        continue;
      }

      if (!inString) {
        if (char === '[') bracketCount++;
        if (char === ']') {
          bracketCount--;
          if (bracketCount === 0) {
            jsonEnd = i;
            break;
          }
        }
      }
    }

    if (jsonEnd !== -1) {
      const jsonStr = stdout.substring(startIndex, jsonEnd + 1);
      try {
        const parsed = JSON.parse(jsonStr);
        if (Array.isArray(parsed)) {
          return parsed;
        }
      } catch (e) {
        // Continue with other methods
      }
    }
  }

  // Strategy 3: Look for empty array
  if (stdout.includes('[]')) {
    const emptyStart = stdout.indexOf('[]');
    if (emptyStart !== -1) {
      return [];
    }
  }

  // Strategy 4: Pure JSON at start
  const trimmed = stdout.trim();
  if (trimmed.startsWith('[') || trimmed.startsWith('{')) {
    try {
      const parsed = JSON.parse(trimmed);
      if (Array.isArray(parsed)) {
        return parsed;
      }
    } catch (e) {
      // Do nothing
    }
  }

  return null;
};

const main = async () => {
  const globalTimeout = setTimeout(() => {
    console.error(JSON.stringify({
      t: 'log',
      l: 'error',
      m: 'Module timeout after 5 minutes'
    }));
    process.exit(1);
  }, 5 * 60 * 1000);

  try {
    const args = parseArgs();

    // Check for curl dependency
    await checkCurlInstalled();

    if (args.proxyInsecure) {
      console.error(JSON.stringify({
        t: 'log',
        l: 'info',
        m: 'PROXY-INSECURE: Using curl with --proxy-insecure flag'
      }));
    }

    if (args.userAgent !== DEFAULT_USER_AGENT) {
      console.error(JSON.stringify({
        t: 'log',
        l: 'info',
        m: `Using custom User-Agent: ${args.userAgent}`
      }));
    }

    // Initialize debug files
    if (args.debug) {
      writeFileSync('debug.txt', `=== DEBUG LOG - ${new Date().toISOString()} ===\n\n`, 'utf8');
      writeFileSync('get_subdomains_performance.log', `=== PERFORMANCE LOG - ${new Date().toISOString()} ===\n\n`, 'utf8');
    }

    console.log(JSON.stringify({
      bmop: '1.0',
      module: 'subdomain-finder-by-certificates',
      version: VERSION,
      pid: process.pid
    }));

    let processedCount = 0;
    let foundCount = 0;
    let errorCount = 0;

    const processBMOPInput = async () => {
      let inputBuffer = '';
      const domainGroups = {};
      const domainsWithoutRoot = new Set();
      let totalDomains = 0;
      const proxies = [];
      
      // Performance metrics
      const performanceMetrics = {
        startTime: Date.now(),
        requests: [],
        successes: 0,
        failures: 0,
        totalBytes: 0
      };

      process.stdin.on('data', (chunk) => {
        inputBuffer += chunk.toString();
        const lines = inputBuffer.split('\n');
        inputBuffer = lines.pop();

        for (const line of lines) {
          const trimmed = line.trim();
          if (!trimmed) continue;

          try {
            const msg = JSON.parse(trimmed);
            if (msg.t === 'd') {
              if (msg.f === 'domain') {
                const domain = msg.v.trim();
                if (domain && domain.includes('.') && !domain.startsWith('*') && isValidDomain(domain)) {
                  const rootDomain = getRootDomain(domain);

                  if (isTLDOrInvalid(rootDomain || domain)) {
                    domainsWithoutRoot.add(domain);
                  } else {
                    if (!domainGroups[rootDomain]) {
                      domainGroups[rootDomain] = [];
                    }
                    domainGroups[rootDomain].push(domain);
                  }
                  totalDomains++;
                }
              } else if (msg.f === 'httpproxy') {
                if (args.useHttpProxies) {
                  const normalizedProxy = normalizeProxy(msg.v);
                  if (normalizedProxy) {
                    proxies.push(normalizedProxy);
                    if (args.debug) {
                      console.error(JSON.stringify({
                        t: 'log',
                        l: 'debug',
                        m: `Loaded proxy: ${normalizedProxy}`
                      }));
                    }
                  }
                }
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

          if (args.useHttpProxies) {
            console.error(JSON.stringify({
              t: 'log',
              l: 'info',
              m: `Loaded ${proxies.length} HTTP proxies for requests`
            }));
          }

          console.error(JSON.stringify({
            t: 'log',
            l: 'info',
            m: `Aggregated ${totalDomains} domains into ${totalRootDomains} root domains (avg ${(totalDomains/totalRootDomains).toFixed(1)} domains/query)`
          }));

          if (domainsWithoutRoot.size > 0) {
            console.error(JSON.stringify({
              t: 'log',
              l: 'info',
              m: `Skipping ${domainsWithoutRoot.size} domains with TLD/invalid root (will emit without query)`
            }));
          }

          const startTime = args.debug ? Date.now() : null;
          let processedDomains = 0;
          let processedRootDomains = 0;
          let lastProgressTime = 0;
          const progressInterval = 1000;

          // Variables for statistics
          const badProxies = new Set();
          let proxyStats = {
            totalProxies: proxies.length,
            badProxiesCount: 0,
            successfulRequests: 0,
            failedRequests: 0,
            requestsWithJson: 0,
            requestsWithoutJson: 0
          };

          // Create fetch function with proxy management
          const createFetchWithCurl = (proxies, insecure = false, useDirectFallback = false) => {
            let proxyIndex = 0;

            return async (url, maxRetries = 10, domainToSearch = null) => {
              let lastError;
              let attemptTimes = [];

              for (let attempt = 1; attempt <= maxRetries; attempt++) {
                const attemptStartTime = Date.now();
                let currentProxy = null;

                // If we have proxies and we're using proxies, use one
                if (proxies.length > 0 && args.useHttpProxies) {
                  // Simple proxy rotation
                  currentProxy = proxies[proxyIndex % proxies.length];
                  proxyIndex++;

                  // If this proxy is marked as bad, skip it
                  if (badProxies.has(currentProxy)) {
                    // Find another proxy that's not bad
                    const goodProxies = proxies.filter(p => !badProxies.has(p));
                    if (goodProxies.length > 0) {
                      currentProxy = goodProxies[Math.floor(Math.random() * goodProxies.length)];
                    } else if (useDirectFallback) {
                      currentProxy = null; // Use direct connection
                    } else {
                      throw new Error('All proxies are marked as bad');
                    }
                  }
                } else if (!args.useHttpProxies) {
                  // Don't use proxies if flag not specified
                  currentProxy = null;
                }

                try {
                  const curlArgs = [
                    '-s',
                    '-L',
                    '-m', '45', // Increased timeout for slow proxies
                    '-H', `User-Agent: ${args.userAgent}`,
                    '-H', 'Accept: application/json',
                  ];

                  if (currentProxy) {
                    if (insecure) {
                      curlArgs.push('--proxy-insecure', currentProxy);
                    } else {
                      curlArgs.push('-x', currentProxy);
                    }

                    if (args.debug) {
                      console.error(JSON.stringify({
                        t: 'log',
                        l: 'debug',
                        m: `Using proxy: ${currentProxy} for ${domainToSearch || 'unknown'}`
                      }));
                    }
                  } else if (args.debug && !args.useHttpProxies) {
                    console.error(JSON.stringify({
                      t: 'log',
                      l: 'debug',
                      m: `Using direct connection for ${domainToSearch || 'unknown'}`
                    }));
                  }

                  curlArgs.push(url);

                  const curlStartTime = Date.now();
                  const { stdout, stderr } = await execFileAsync('curl', curlArgs, {
                    maxBuffer: 50 * 1024 * 1024 // Increased buffer for large responses
                  });
                  const curlEndTime = Date.now();
                  const curlDuration = curlEndTime - curlStartTime;

                  performanceMetrics.totalBytes += stdout ? stdout.length : 0;
                  performanceMetrics.requests.push({
                    attempt,
                    proxy: currentProxy || 'direct',
                    duration: curlDuration,
                    responseSize: stdout ? stdout.length : 0,
                    domain: domainToSearch
                  });

                  attemptTimes.push({
                    attempt,
                    proxy: currentProxy || 'direct',
                    duration: curlDuration,
                    responseSize: stdout ? stdout.length : 0
                  });

                  if (args.debug) {
                    const fullCommand = `curl ${curlArgs.join(' ')}`;
                    appendFileSync('debug.txt', `\n${'='.repeat(80)}\n`, 'utf8');
                    appendFileSync('debug.txt', `ATTEMPT ${attempt}/${maxRetries}\n`, 'utf8');
                    appendFileSync('debug.txt', `Time: ${new Date().toISOString()}\n`, 'utf8');
                    appendFileSync('debug.txt', `Duration: ${curlDuration}ms\n`, 'utf8');
                    appendFileSync('debug.txt', `COMMAND:\n${fullCommand}\n\n`, 'utf8');
                    appendFileSync('debug.txt', `STDOUT (${stdout.length} bytes):\n${stdout.substring(0, 10000)}${stdout.length > 10000 ? '\n...[truncated]...' : ''}\n\n`, 'utf8');
                    if (stderr) {
                      appendFileSync('debug.txt', `STDERR:\n${stderr}\n\n`, 'utf8');
                    }
                  }

                  if (stdout && stdout.length > 0) {
                    // Always look for JSON regardless of surrounding content
                    const hasIssuerCaId = stdout.includes('"issuer_ca_id"');

                    if (hasIssuerCaId) {
                      proxyStats.requestsWithJson++;
                      const extractedJson = extractJsonFromMixedResponse(stdout);

                      if (extractedJson !== null) {
                        // Success: found valid JSON
                        if (currentProxy && badProxies.has(currentProxy)) {
                          // This proxy was marked as bad but worked
                          badProxies.delete(currentProxy);
                          if (args.debug) {
                            console.error(JSON.stringify({
                              t: 'log',
                              l: 'info',
                              m: `Proxy ${currentProxy} is working again, removing from bad list`
                            }));
                          }
                        }

                        proxyStats.successfulRequests++;
                        performanceMetrics.successes++;
                        return { ok: true, data: extractedJson };
                      } else {
                        // Has "issuer_ca_id" but couldn't extract JSON
                        // Could be a proxy that truncates the response
                        if (currentProxy) {
                          badProxies.add(currentProxy);
                          proxyStats.badProxiesCount = badProxies.size;
                        }
                        proxyStats.failedRequests++;
                        performanceMetrics.failures++;
                        throw new Error('Found "issuer_ca_id" but could not extract valid JSON');
                      }
                    } else {
                      // Doesn't have "issuer_ca_id", could be an error or empty response
                      proxyStats.requestsWithoutJson++;

                      // Check for empty array
                      if (stdout.trim() === '[]') {
                        return { ok: true, data: [] };
                      }

                      // Check for crt.sh error
                      if (stdout.includes('<TITLE>crt.sh | ERROR!</TITLE>')) {
                        const hasTooManyResults = stdout.includes('Unfortunately, searches that would produce many results may never succeed');
                        if (hasTooManyResults) {
                          throw new Error('crt.sh error: Too many results - query too broad');
                        } else {
                          throw new Error('crt.sh server error');
                        }
                      }

                      // Check for rate limiting
                      if (stdout.includes('429 Too Many Requests') || stdout.includes('Rate limit exceeded')) {
                        if (currentProxy) {
                          badProxies.add(currentProxy);
                          proxyStats.badProxiesCount = badProxies.size;
                        }
                        // Add exponential backoff for rate limiting
                        const delay = Math.min(5000, 1000 * Math.pow(2, attempt));
                        await sleep(delay);
                        throw new Error('Rate limited');
                      }

                      // If it doesn't have "issuer_ca_id" and is HTML, probably a bad proxy
                      if (currentProxy && (stdout.includes('<!DOCTYPE') || stdout.includes('<html'))) {
                        badProxies.add(currentProxy);
                        proxyStats.badProxiesCount = badProxies.size;
                      }

                      // If we get here, assume no results
                      return { ok: true, data: [] };
                    }
                  }

                  throw new Error('Empty response from curl');

                } catch (error) {
                  lastError = error;
                  if (args.debug && attempt === maxRetries) {
                    console.error(JSON.stringify({
                      t: 'log',
                      l: 'debug',
                      m: `Request failed for ${url}: ${error.message} (proxy: ${currentProxy || 'direct'})`
                    }));
                  }

                  // If it's a specific proxy that failed, mark it as bad
                  if (currentProxy && (error.message.includes('Could not resolve host') ||
                      error.message.includes('Connection refused') ||
                      error.message.includes('Timeout'))) {
                    badProxies.add(currentProxy);
                    proxyStats.badProxiesCount = badProxies.size;
                  }

                  // Exponential backoff
                  if (attempt < maxRetries) {
                    const backoffMs = Math.min(1000 * Math.pow(2, attempt - 1), 10000);
                    await sleep(backoffMs);
                  }
                }
              }

              // If we get here, all attempts failed
              proxyStats.failedRequests++;
              performanceMetrics.failures++;
              if (args.debug) {
                const totalDuration = Date.now() - attemptTimes[0].attemptStartTime;
                appendFileSync('get_subdomains_performance.log',
                  `FAILED: ${url}\n` +
                  `  Domain: ${domainToSearch || 'unknown'}\n` +
                  `  Total attempts: ${maxRetries}\n` +
                  `  Error: ${lastError.message}\n\n`,
                  'utf8'
                );
              }

              throw lastError;
            };
          };

          // Create fetch function
          // If --use-http-proxies is specified, DO NOT use direct fallback
          const fetchWithRetry = createFetchWithCurl(proxies, args.proxyInsecure, false);

          const getSubdomainsFromCrt = async (rootDomain) => {
            const domainStartTime = Date.now();
            const cleanDomain = rootDomain.toLowerCase().trim();
            const queryDomain = cleanDomain.startsWith('.') ? cleanDomain : `.${cleanDomain}`;
            const url = `https://crt.sh/?q=${encodeURIComponent(queryDomain)}&output=json`;

            try {
              const response = await fetchWithRetry(url, 10, cleanDomain);
              if (!response.ok) return [];
              const data = response.data;
              if (!Array.isArray(data)) return [];

              const subdomains = new Set();

              for (const entry of data) {
                if (entry.name_value) {
                  const names = typeof entry.name_value === 'string'
                    ? entry.name_value.split('\n')
                    : [entry.name_value];
                  for (const name of names) {
                    const cleanName = name
                      .trim()
                      .toLowerCase()
                      .replace(/^\*\./, '') // Remove wildcards
                      .replace(/^\.+/, '')  // Remove leading dots
                      .replace(/\.+$/, ''); // Remove trailing dots
                    
                    // Add any subdomain that contains the root domain
                    if (cleanName && isValidDomain(cleanName) && 
                        (cleanName === cleanDomain || cleanName.endsWith('.' + cleanDomain))) {
                      subdomains.add(cleanName);
                    }
                  }
                }
              }

              if (args.debug) {
                const domainDuration = Date.now() - domainStartTime;
                appendFileSync('get_subdomains_performance.log',
                  `DOMAIN COMPLETE: ${cleanDomain}\n` +
                  `  Time: ${domainDuration}ms (${(domainDuration/1000).toFixed(2)}s)\n` +
                  `  Subdomains found: ${subdomains.size}\n` +
                  `  Records processed: ${data.length}\n\n`,
                  'utf8'
                );
              }

              return Array.from(subdomains);
            } catch (error) {
              if (args.debug) {
                const domainDuration = Date.now() - domainStartTime;
                appendFileSync('get_subdomains_performance.log',
                  `DOMAIN FAILED: ${cleanDomain}\n` +
                  `  Time: ${domainDuration}ms (${(domainDuration/1000).toFixed(2)}s)\n` +
                  `  Error: ${error.message}\n\n`,
                  'utf8'
                );
              }

              console.error(JSON.stringify({
                t: 'log',
                l: 'error',
                m: `Failed to fetch crt.sh for ${cleanDomain}: ${error.message}`
              }));
              return [];
            }
          };

          emitProgress(0, totalDomains, 0, totalRootDomains, args.debug, startTime);

          // Emit domains without root first
          for (const domain of domainsWithoutRoot) {
            emitBatch([domain]);
            processedDomains++;
            processedCount++;
            foundCount++;

            const now = Date.now();
            if (now - lastProgressTime > progressInterval) {
              emitProgress(processedDomains, totalDomains, processedRootDomains, totalRootDomains, args.debug, startTime);
              lastProgressTime = now;
            }
          }

          // Process each root domain
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
              // Add all matching subdomains
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

          // Write performance summary
          if (args.debug) {
            const totalExecutionTime = Date.now() - startTime;
            appendFileSync('get_subdomains_performance.log',
              `\n${'='.repeat(80)}\n` +
              `EXECUTION SUMMARY\n` +
              `${'='.repeat(80)}\n` +
              `Total execution time: ${totalExecutionTime}ms (${(totalExecutionTime/1000).toFixed(2)}s / ${(totalExecutionTime/60000).toFixed(2)}min)\n` +
              `Total domains processed: ${totalDomains}\n` +
              `Total root domains queried: ${totalRootDomains}\n` +
              `Average time per root domain: ${(totalExecutionTime/totalRootDomains).toFixed(0)}ms\n` +
              `Total subdomains found: ${foundCount}\n` +
              `Total errors: ${errorCount}\n` +
              `\nPerformance metrics:\n` +
              `  - Total requests: ${performanceMetrics.requests.length}\n` +
              `  - Successful requests: ${performanceMetrics.successes}\n` +
              `  - Failed requests: ${performanceMetrics.failures}\n` +
              `  - Total bytes transferred: ${performanceMetrics.totalBytes}\n` +
              `  - Average request time: ${performanceMetrics.requests.reduce((sum, r) => sum + r.duration, 0) / performanceMetrics.requests.length}ms\n` +
              `\nProxy statistics:\n` +
              `  - Total proxies loaded: ${proxyStats.totalProxies}\n` +
              `  - Proxies marked as bad: ${badProxies.size}\n` +
              `  - Successful requests: ${proxyStats.successfulRequests}\n` +
              `  - Failed requests: ${proxyStats.failedRequests}\n` +
              `  - Requests with JSON: ${proxyStats.requestsWithJson}\n` +
              `  - Requests without JSON: ${proxyStats.requestsWithoutJson}\n` +
              `\nModule configuration:\n` +
              `  - JSON extraction improved with regex and start marker detection\n` +
              `  - Using proxies: ${args.useHttpProxies ? 'YES' : 'NO'}\n` +
              `  - Direct fallback: ${false}\n` +
              `  - User-Agent: ${args.userAgent.substring(0, 50)}...\n`,
              'utf8'
            );
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

      const fetchWithRetry = async (url, maxRetries = 2) => {
        let lastError;
        for (let attempt = 1; attempt <= maxRetries; attempt++) {
          try {
            const curlArgs = [
              '-s',
              '-L',
              '-m', '15',
              '-H', `User-Agent: ${args.userAgent}`,
              '-H', 'Accept: application/json',
              url
            ];

            const { stdout } = await execFileAsync('curl', curlArgs, {
              maxBuffer: 10 * 1024 * 1024
            });

            if (stdout && stdout.trim()) {
              const extractedJson = extractJsonFromMixedResponse(stdout);
              if (extractedJson !== null) {
                return { ok: true, data: extractedJson };
              }

              throw new Error('Could not extract JSON from response');
            }

            throw new Error('Empty response');
          } catch (error) {
            lastError = error;
            if (attempt < maxRetries) {
              await sleep(Math.pow(2, attempt) * 1000);
            }
          }
        }
        throw lastError;
      };

      processedCount++;
      const cleanDomain = args.domain.toLowerCase().trim();
      const rootDomain = getRootDomain(cleanDomain);

      if (isTLDOrInvalid(rootDomain || cleanDomain)) {
        console.error(JSON.stringify({
          t: 'log',
          l: 'warn',
          m: `Domain ${cleanDomain} appears to be a TLD or invalid - skipping crt.sh query`
        }));
        emitBatch([cleanDomain]);
        foundCount++;
      } else {
        const queryDomain = cleanDomain.startsWith('.') ? cleanDomain : `.${cleanDomain}`;
        const url = `https://crt.sh/?q=${encodeURIComponent(queryDomain)}&output=json`;

        try {
          const response = await fetchWithRetry(url);
          if (response.ok) {
            const data = response.data;
            if (Array.isArray(data)) {
              const subdomains = new Set();
              subdomains.add(cleanDomain);

              for (const entry of data) {
                if (entry.name_value) {
                  const names = typeof entry.name_value === 'string'
                    ? entry.name_value.split('\n')
                    : [entry.name_value];
                  for (const name of names) {
                    const cleanName = name
                      .trim()
                      .toLowerCase()
                      .replace(/^\*\./, '')
                      .replace(/^\.+/, '')
                      .replace(/\.+$/, '');
                    
                    if (cleanName && isValidDomain(cleanName) && cleanName.includes(cleanDomain)) {
                      subdomains.add(cleanName);
                    }
                  }
                }
              }

              foundCount += subdomains.size;
              emitBatch(Array.from(subdomains).sort());
            }
          }
        } catch (error) {
          errorCount++;
          console.error(JSON.stringify({
            t: 'log',
            l: 'error',
            m: `Failed to fetch crt.sh for ${cleanDomain}: ${error.message}`
          }));
          emitBatch([cleanDomain]);
          foundCount++;
        }
      }
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
    
  } catch (err) {
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
  } finally {
    clearTimeout(globalTimeout);
  }
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