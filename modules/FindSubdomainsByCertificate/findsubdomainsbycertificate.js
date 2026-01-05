#!/usr/bin/env node
// Name: Subdomain Finder by Certificates
// Description: Finds subdomains using SSL certificate transparency logs (crt.sh)
// Type: processor
// Stage: 1
// Consumes: *
// Provides: subdomain
// Storage: delete
// Install: npm i simpleargumentsparser
// InstallScope: shared
// RateLimit: 100
// Args: -d, --domain <domain> (optional) Target domain for standalone mode
// Args: --debug (flag, optional) Enable debug mode with ETA calculations

import readline from "readline";
import * as exec from "child_process";

console.log(JSON.stringify({ bmop: "1.0", module: "subdomain-finder", pid: process.pid }));

let domains = [];

const rl = readline.createInterface({
  input: process.stdin,
  terminal: false
});

rl.on("line", line => {
  if (!line.trim()) return;

  try {
    const jsonL = JSON.parse(line);
    if (jsonL.v) {
      if (jsonL.f === "domain") {
        domains.push(jsonL.v);
      }
    }
  } catch (err) {
    console.error(JSON.stringify({ 
      t: "error", 
      code: "PARSE_ERROR", 
      m: "Unable to parse JSON line: " + err.message 
    }));
  }
});

const run = args => {
  try {
    let res = exec.execSync(args, { maxBuffer: 10 * 1024 * 1024 }).toString();
    return res;
  } catch (err) {
    return null;
  }
};

const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));

const parseResponse = res => {
  if (!res) return { error: "NO_RESPONSE" };

  try {
    const inLines = res.split("\n");
    const firstLine = inLines[0].trim();
    let lastLine = "";
    if (inLines.length > 3) {
      lastLine = inLines[inLines.length - 2]?.trim();
    }

    // Check for 502 Bad Gateway
    if (/\<html\>/gi.test(firstLine) && /\<\/html\>/gi.test(lastLine) && /502 Bad Gateway/gi.test(res)) {
      return { error: "BAD_GATEWAY" };
    }

    // Check for rate limit (429)
    /* if (res.includes("429") || /rate limit/gi.test(res)) {
      return { error: "RATE_LIMIT" };
    } */

    const jsonRes = JSON.parse(res);
    return { data: jsonRes };
  } catch (err) {
    // Check if it's HTML error response
    if (err instanceof SyntaxError && err.message.includes('<')) {
      if (res.includes("429") || /rate limit/gi.test(res)) {
        //console.log(`DEBUG 429_RESPONSE: ${res}\n\n\n`);
        return { error: "RATE_LIMIT" };
      }

      if (res.includes("<TITLE>crt.sh | ERROR!</TITLE>")) {
        //console.log(`Found crt.sh internal Error quering the domain`);
        return { error: "INTERNAL_ERROR" }
      }
      //console.log(`DEBUG HTML_RESPONSE: ${res}\n\n\n`);
      return { error: "HTML_RESPONSE" };
    }
    //console.log(`DEBUG UNKNOWN_RESPONSE: ${res}\n\n\n`);
    return { error: "PARSE_ERROR", message: err.message };
  }
};

const getSubdomains = jsonRes => {
  const subdomains = [];
  
  for (let i in jsonRes) {
    let aux = (jsonRes[i].name_value.split("\n") || jsonRes[i].name_value);
    for (let j in aux) {
      if (aux[j].startsWith("*.")) {
        aux[j] = aux[j].slice(2);
      }
      subdomains.push(aux[j]);
    }
  }

  return [...new Set(subdomains)];
};

const makeCommandAndRequest = async (domainAddr, retryCount = 0) => {
  const MAX_RETRIES = 3;
  
  try {
    const command = `curl -s -w "\\n%{http_code}" 'https://crt.sh?q=.${encodeURIComponent(domainAddr)}&output=json'`;
    const res = run(command);
    
    if (!res) {
      console.error(JSON.stringify({
        t: "error",
        code: "CURL_FAILED",
        m: `Failed to execute curl for domain: ${domainAddr}`
      }));
      return [];
    }

    // Split response and HTTP code
    const lines = res.trim().split("\n");
    const httpCode = lines[lines.length - 1];
    const responseBody = lines.slice(0, -1).join("\n");

    // Check for rate limit
    if (httpCode === "429") {
      console.error(JSON.stringify({
        t: "log",
        l: "warn",
        m: `Rate limit hit for domain ${domainAddr}. Pausing for 4 minutes...`
      }));
      
      await sleep(240000); // 4 minutes
      
      console.error(JSON.stringify({
        t: "log",
        l: "info",
        m: `Resuming after rate limit pause. Retrying domain: ${domainAddr}`
      }));
      
      return await makeCommandAndRequest(domainAddr, retryCount);
    }

    const parsed = parseResponse(responseBody);

    // Handle rate limit detected in body
    if (parsed.error === "RATE_LIMIT") {
      console.error(JSON.stringify({
        t: "log",
        l: "warn",
        m: `Rate limit detected for domain ${domainAddr}. Pausing for 4 minutes...`
      }));
      
      await sleep(240000); // 4 minutes
      
      console.error(JSON.stringify({
        t: "log",
        l: "info",
        m: `Resuming after rate limit pause. Retrying domain: ${domainAddr}`
      }));
      
      return await makeCommandAndRequest(domainAddr, retryCount);
    }

    // Handle other errors
    if (parsed.error) {
      if (retryCount < MAX_RETRIES && parsed.error !== "BAD_GATEWAY") {
        console.error(JSON.stringify({
          t: "log",
          l: "warn",
          m: `Error ${parsed.error} for ${domainAddr}, retrying (${retryCount + 1}/${MAX_RETRIES})...`
        }));
        await sleep(2000); // Wait 2 seconds before retry
        return await makeCommandAndRequest(domainAddr, retryCount + 1);
      }
      
      console.error(JSON.stringify({
        t: "error",
        code: parsed.error,
        m: `Failed to fetch subdomains for ${domainAddr}: ${parsed.message || parsed.error}`
      }));
      return [];
    }

    if (!parsed.data || parsed.data.length === 0) {
      console.error(JSON.stringify({
        t: "log",
        l: "info",
        m: `No subdomains found for domain: ${domainAddr}`
      }));
      return [];
    }

    const subdomains = getSubdomains(parsed.data);
    return subdomains;
    
  } catch (err) {
    console.error(JSON.stringify({
      t: "error",
      code: "UNEXPECTED_ERROR",
      m: `Unexpected error for ${domainAddr}: ${err.message}`
    }));
    return [];
  }
};

const main = async () => {
  console.error(JSON.stringify({ 
    t: "log", 
    l: "info", 
    m: `Processing ${domains.length} domains` 
  }));
  
  let totalSubdomains = 0;
  
  for (let i = 0; i < domains.length; i++) {
    const domain = domains[i];
    
    console.error(JSON.stringify({
      t: "progress",
      c: i + 1,
      T: domains.length,
      m: `Processing ${domain}`
    }));
    
    const subdomains = await makeCommandAndRequest(domain);
    
    if (subdomains.length > 0) {
      // Output batch for THIS domain
      console.log(JSON.stringify({ 
        t: "batch", 
        f: "subdomain", 
        c: subdomains.length 
      }));
      
      subdomains.forEach(subdomain => {
        console.log(subdomain);
      });
      
      console.log(JSON.stringify({ t: "batch_end" }));
      
      totalSubdomains += subdomains.length;
      
      console.error(JSON.stringify({
        t: "log",
        l: "info",
        m: `Found ${subdomains.length} subdomains for ${domain}`
      }));
    }
  }
  
  console.log(JSON.stringify({ 
    t: "result", 
    ok: true, 
    count: totalSubdomains 
  }));
};

rl.on("close", () => {
  main().catch(err => {
    console.error(JSON.stringify({
      t: "error",
      code: "FATAL",
      m: err.message,
      fatal: true
    }));
    console.log(JSON.stringify({ 
      t: "result", 
      ok: false, 
      error: err.message 
    }));
    process.exit(1);
  });
});
