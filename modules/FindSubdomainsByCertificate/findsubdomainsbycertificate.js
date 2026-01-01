#!/usr/bin/env node
// Name: Subdomain Finder by Certificates
// Description: Finds subdomains using SSL certificate transparency logs (crt.sh) with TLD-aware grouping
// Type: processor
// Stage: 1
// Consumes: *
// Provides: subdomain
// Storage: delete
// Install: npm i simpleargumentsparser
// InstallScope: shared
// RateLimit: 100
// Timeout: 30000
// Args: -d, --domain <domain> (optional) Target domain for standalone mode
// Args: --debug (flag, optional) Enable debug mode with ETA calculations
// Args: --use-http-proxies (flag, optional) Use HTTP proxies to avoid rate limiting
// Args: --proxy-insecure (flag, optional) Disable TLS certificate validation for proxies
// Args: --user-agent <agent> (optional) Custom User-Agent string

import readline from "readline";
import * as exec from "child_process";

console.log(JSON.stringify({ bmop: "1.0", module: "subdomain-finder", pid: process.pid }));

let domains = [];
let proxies = [];
let subdomains = [];

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
      } else if (jsonL.f === "httpproxy") {
        proxies.push(jsonL.v);
      }
    }
  } catch (err) {
    console.log(JSON.stringify({ t: "log", l: "debug", m: "Unable to parse a JSON line: " + err.message }));
  }
});



const run = args => { // Make async
  let res = exec.execSync(args).toString()
  return res;
};

const parseResponse = res => {
  try {
    const inLines = res.split("\n");
    console.log("INLINESLENGTH:", inLines.length);
    const firstLine = inLines[0].trim();
    let lastLine = "";
    if (inLines.length > 3) { 
      lastLine = inLines[inLines.length - 2]?.trim();
    }
    console.log(`First Line: "${firstLine}"\nLast Line: "${lastLine}"`);

    if (/\<html\>/gi.test(firstLine) && /\<\/html\>/gi.test(lastLine) && /502 Bad Gateway/gi.test(res) ) {
      console.log("BAD GATEWAAAAY, WANT TO RERUN OR CHANGE PROXY");
      return false;
    }

    const jsonRes = JSON.parse(res);
    return jsonRes;
  } catch (err) {
    console.log(`DEBUG ERR: ${err}`);

    if (err instanceof SyntaxError && err.message.includes('<')) {
      
    }
    return {};
  }
}

const getSubdomains = jsonRes => {
  for (let i in jsonRes) {
    let aux = (jsonRes[i].name_value.split("\n") || jsonRes[i].name_value);
    for (let j in aux) {
      if (aux[j].startsWith("*.")) {
        aux[j] = aux[j].slice(2);
      }
      subdomains.push(aux[j]);
      console.log(`AUX[J] ES: ${aux[j]}`);
    }
  }


  subdomains = [...new Set(subdomains)];

  if (!subdomains) {
    console.log("no subdomains found");
  }

}

const main = () => {
  const command = `curl --proxy-insecure 'https://${proxies[Math.floor(Math.random() * proxies.length)]}' 'https://crt.sh?q=.${encodeURIComponent("termux.org")}&output=json' `;
  const res = run(command);
  console.log(`Response is:\n${res}\n`);


  console.log("\n\nParsing response as JSON ...");
  const jsonRes = parseResponse(res);
  console.log(`JsonRes is:\n${jsonRes}`);

  getSubdomains(jsonRes);
  console.log(subdomains);
}


rl.on("close", () => {
  console.log(JSON.stringify({ t: "log", l: "info", m: `Found ${domains.length} domains and ${proxies.length} proxies` }));
  main(); 
});
