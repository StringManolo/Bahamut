#!/usr/bin/env node

// Name: Check Tor
// Description: Check if tor is running
// Install: npm install socks-proxy-agent node-fetch

import fetch from 'node-fetch';
import { SocksProxyAgent } from 'socks-proxy-agent';

const agent = new SocksProxyAgent('socks5h://127.0.0.1:9050');

async function checkTor() {
  try {
    const response = await fetch('https://check.torproject.org', { agent });
    const text = await response.text();

    if (text.includes('Congratulations')) {
      console.log('Tor Working');
    } else {
      console.log('Tor Not Working');
    }
  } catch (error) {
    console.log('Tor Not Working cuz: ' + error);
  }
}

checkTor();
