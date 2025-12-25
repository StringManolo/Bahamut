#!/usr/bin/env node

// Name: Wildcard Domain Cleaner
// Description: Removes wildcard prefixes and deduplicates domains
// Type: processor
// Stage: 2
// Consumes: domain
// Provides: domain
// Storage: replace
// Install: 
// InstallScope: global

const fs = require('fs');

console.log(JSON.stringify({bmop:"1.0",module:"wildcard-cleaner",pid:process.pid}));

const domains = new Set();
let buffer = '';
let inputCount = 0;
let jsonErrors = 0;

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
        let domain = msg.v;
        inputCount++;

        // shit parse. Example module. 
        if (domain.startsWith('*.')) domain = domain.substring(2);
        if (domain.startsWith('www.')) domain = domain.substring(4);

        if (domain && domain.length > 3 && domain.includes('.')) {
          domains.add(domain.toLowerCase());
        }
      }
    } catch (e) {
      jsonErrors++;
      fs.writeSync(2, `[DEBUG] JSON Parse Error: ${e.message} | Line: ${trimmed}\n`);
    }
  }
});

process.stdin.on('end', () => {
  fs.writeSync(2, `[DEBUG] Stdin ended. Total received: ${inputCount}, JSON Errors: ${jsonErrors}\n`);
  console.error(JSON.stringify({t:"log",l:"info",m:`Cleaned ${inputCount} domains into ${domains.size} unique domains`}));

  const sorted = Array.from(domains).sort();

  console.log(JSON.stringify({t:"batch",f:"domain",c:sorted.length}));
  sorted.forEach(domain => {
    process.stdout.write(domain + '\n');
  });
  console.log(JSON.stringify({t:"batch_end"}));

  console.log(JSON.stringify({t:"result",ok:true,count:sorted.length}));
});
