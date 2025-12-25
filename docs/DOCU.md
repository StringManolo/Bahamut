# Documentation

## Install & Run
```bash
git clone https://github.com/stringmanolo/bahamut
cd bahamut
make
./bahamut
```

## Optional Dependencies
#### Chafa
If `chafa` is installed, Bahamut will render a graphical logo directly in the terminal on startup.

## APIs

### Multilanguage Modules
#### Example
```javascript
#!/usr/bin/env node

// Name: Check Tor
// Description: Check if tor is running
// Install: npm install socks-proxy-agent node-fetch
// InstallScope: isolated 

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
```


#### Declarations
Modules need the next components:
* Shebang
* Name
* Description
* Install
* InstallScope

##### Shebang


#### Input Format

#### Output Format

#### Advanced Wrappers
