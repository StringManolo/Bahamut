#!/usr/bin/env node
// Name: Check Tor
// Description: Check if tor is running by sending a request to https://check.torproject.org
// Type: checker
// Stage: 0
// Provides: tor_status
// Install: npm install socks-proxy-agent node-fetch
// InstallScope: shared

import fetch from 'node-fetch';
import { SocksProxyAgent } from 'socks-proxy-agent';

console.log(JSON.stringify({bmop:"1.0",module:"check-tor",pid:process.pid}));

const agent = new SocksProxyAgent('socks5h://127.0.0.1:9050');

async function checkTor() {
  try {
    console.error(JSON.stringify({t:"log",l:"info",m:"Checking Tor connection..."}));

    const response = await fetch('https://check.torproject.org', { agent, timeout: 10000 });
    const text = await response.text();

    if (text.includes('Congratulations')) {
      console.error(JSON.stringify({t:"log",l:"info",m:"Tor is working correctly"}));
      console.log(JSON.stringify({t:"d",f:"tor_status",v:"working"}));
      console.log(JSON.stringify({t:"result",ok:true}));
    } else {
      console.error(JSON.stringify({t:"log",l:"warn",m:"Tor proxy connected but not working properly"}));
      console.log(JSON.stringify({t:"d",f:"tor_status",v:"not_working"}));
      console.log(JSON.stringify({t:"result",ok:false,error:"Tor not working"}));
    }
  } catch (error) {
    console.error(JSON.stringify({t:"error",code:"TOR_FAILED",m:error.message}));
    console.log(JSON.stringify({t:"d",f:"tor_status",v:"failed"}));
    console.log(JSON.stringify({t:"result",ok:false,error:error.message}));
    process.exit(1);
  }
}

checkTor();
