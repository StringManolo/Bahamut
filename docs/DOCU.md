# Bahamut Documentation

## Install & Run
```bash
git clone https://github.com/stringmanolo/bahamut
cd bahamut
make
./bahamut
```

## Optional Dependencies

### Chafa
If `chafa` is installed, Bahamut will render a graphical logo directly in the terminal on startup.

---

## Module System

### Creating Modules

Bahamut supports modules written in multiple languages: **JavaScript (Node.js)**, **Python**, and **Bash** __More languages will be supported in the future__.

#### Module Structure

Every module must include:

1. **Shebang** - Specifies the interpreter
2. **Name** - Human-readable module name
3. **Description** - Brief description of functionality
4. **Install** - Command to install dependencies
5. **InstallScope** - Where to install dependencies (`shared`, `isolated`, or `global`)

#### Shebang

The shebang line tells Bahamut which interpreter to use:

```bash
#!/usr/bin/env node          # For Node.js modules
#!/usr/bin/env python3       # For Python modules
#!/usr/bin/env bash          # For Bash modules
```

You can also specify Python version:
```bash
#!/usr/bin/env python3.9     # Uses Python 3.9 specifically
#!/usr/bin/env python3.11    # Uses Python 3.11 specifically
```

#### Name

Provides a human-readable name for the module:
```javascript
// Name: Check Tor Connection
```

#### Description

Brief explanation of what the module does:
```javascript
// Description: Verifies if Tor proxy is working correctly
```

#### Install

Command to install module dependencies:

**Node.js:**
```javascript
// Install: npm install socks-proxy-agent node-fetch
```

**Python:**
```python
# Install: pip install requests beautifulsoup4
```

**Bash:**
```bash
# Install: apt-get install curl jq
```

#### InstallScope

Defines where dependencies are installed:

- **`shared`** - Dependencies shared across all modules (recommended for common packages)
- **`isolated`** - Dependencies only for this module (use for conflicting versions)
- **`global`** - System-wide installation (use sparingly)

```javascript
// InstallScope: shared
```

---

## BMOP - Bahamut Module Output Protocol

BMOP is a lightweight protocol for module communication using JSON Lines (JSONL). Each line is a complete JSON object.

### Protocol Version

Every module should start with a protocol identifier:

```json
{"bmop":"1.0","module":"mymodule","pid":12345}
```

**Fields:**
- `bmop` - Protocol version (currently "1.0")
- `module` - Module name
- `pid` - Process ID (optional)

### Message Types

#### 1. Log Messages (`log`)

For informational output, warnings, and debugging:

```json
{"t":"log","l":"info","m":"Starting data collection..."}
{"t":"log","l":"warn","m":"Rate limit approaching"}
{"t":"log","l":"error","m":"API request failed"}
{"t":"log","l":"debug","m":"Processing item 42"}
```

**Fields:**
- `t` - Type: `"log"`
- `l` - Level: `"info"`, `"warn"`, `"error"`, `"debug"`
- `m` - Message text

**Note:** Log messages go to stderr, not stdout (data only goes to stdout).

#### 2. Progress Updates (`progress`)

For long-running operations:

```json
{"t":"progress","c":150,"T":798}
{"t":"progress","c":300,"T":798,"m":"Processing programs..."}
```

**Fields:**
- `t` - Type: `"progress"`
- `c` - Current progress
- `T` - Total count
- `m` - Optional status message

#### 3. Data Output (`d`)

The primary output - actual data from the module:

```json
{"t":"d","f":"domain","v":"example.com"}
{"t":"d","f":"url","v":"https://example.com/api"}
{"t":"d","f":"ip","v":"192.168.1.1"}
{"t":"d","f":"email","v":"admin@example.com"}
```

**Fields:**
- `t` - Type: `"d"` (data)
- `f` - Format/type of data (`domain`, `url`, `ip`, `subdomain`, `email`, `port`, etc.)
- `v` - The actual value

**Common formats:**
- `domain` - Root domains (example.com)
- `subdomain` - Subdomains (api.example.com)
- `url` - Full URLs
- `ip` - IP addresses
- `port` - Port numbers
- `email` - Email addresses
- `hash` - Hashes (MD5, SHA256, etc.)
- `vulnerability` - Security findings
- `credential` - Credentials (username/password pairs)

#### 4. Batch Mode (`batch`)

For outputting many items efficiently:

```json
{"t":"batch","f":"domain","c":1000}
example1.com
example2.com
example3.com
...
example1000.com
{"t":"batch_end"}
```

**Fields:**
- `t` - Type: `"batch"`
- `f` - Format of data
- `c` - Count of items in batch
- `"batch_end"` - Marks end of batch

**Use batch mode when:**
- Outputting 100+ items
- Performance is critical
- Data is simple (single values per line)

#### 5. Result Summary (`result`)

Final status of module execution:

```json
{"t":"result","ok":true,"count":35386,"time":2.3}
{"t":"result","ok":false,"error":"API authentication failed"}
```

**Fields:**
- `t` - Type: `"result"`
- `ok` - Success status (boolean)
- `count` - Number of items returned (optional)
- `time` - Execution time in seconds (optional)
- `error` - Error message if `ok` is false

#### 6. Error Messages (`error`)

For critical errors:

```json
{"t":"error","code":"AUTH_FAILED","m":"Invalid API key"}
{"t":"error","code":"NETWORK","m":"Connection timeout","fatal":true}
```

**Fields:**
- `t` - Type: `"error"`
- `code` - Error code
- `m` - Error message
- `fatal` - If true, module cannot continue (optional)

---

## Implementation Examples

### Node.js Module

```javascript
#!/usr/bin/env node
// Name: Domain Scanner
// Description: Scans for domains from multiple sources
// Install: npm install axios
// InstallScope: shared

import axios from 'axios';

console.log(JSON.stringify({bmop:"1.0",module:"domain-scanner",pid:process.pid}));

async function scanDomains() {
  console.error(JSON.stringify({t:"log",l:"info",m:"Starting scan..."}));
  
  try {
    const response = await axios.get('https://api.example.com/domains');
    const domains = response.data;
    
    console.error(JSON.stringify({t:"progress",c:0,T:domains.length}));
    
    domains.forEach((domain, i) => {
      console.log(JSON.stringify({t:"d",f:"domain",v:domain}));
      
      if ((i + 1) % 100 === 0) {
        console.error(JSON.stringify({t:"progress",c:i+1,T:domains.length}));
      }
    });
    
    console.log(JSON.stringify({t:"result",ok:true,count:domains.length}));
  } catch (error) {
    console.error(JSON.stringify({t:"error",code:"FETCH_FAILED",m:error.message,fatal:true}));
    console.log(JSON.stringify({t:"result",ok:false,error:error.message}));
    process.exit(1);
  }
}

scanDomains();
```

### Python Module

```python
#!/usr/bin/env python3
# Name: Subdomain Finder
# Description: Finds subdomains using DNS queries
# Install: pip install dnspython
# InstallScope: shared

import json
import sys
import dns.resolver

print(json.dumps({"bmop":"1.0","module":"subdomain-finder","pid":os.getpid()}))

def log(level, message):
  print(json.dumps({"t":"log","l":level,"m":message}), file=sys.stderr)

def data(format_type, value):
  print(json.dumps({"t":"d","f":format_type,"v":value}))

def result(success, **kwargs):
  print(json.dumps({"t":"result","ok":success, **kwargs}))

try:
  log("info", "Starting subdomain enumeration")
  
  domain = "example.com"
  subdomains = ["www", "api", "mail", "ftp"]
  
  found = 0
  for i, sub in enumerate(subdomains):
    try:
      full_domain = f"{sub}.{domain}"
      answers = dns.resolver.resolve(full_domain, 'A')
      
      for rdata in answers:
        data("subdomain", full_domain)
        data("ip", str(rdata))
        found += 1
        
    except dns.resolver.NXDOMAIN:
      pass
    
    if (i + 1) % 10 == 0:
      print(json.dumps({"t":"progress","c":i+1,"T":len(subdomains)}), file=sys.stderr)
  
  result(True, count=found)
  
except Exception as e:
  log("error", str(e))
  result(False, error=str(e))
  sys.exit(1)
```

### Bash Module

```bash
#!/usr/bin/env bash
# Name: Port Scanner
# Description: Simple port scanner
# Install: 
# InstallScope: global

echo '{"bmop":"1.0","module":"port-scanner","pid":'$$'}'

log() {
  echo "{\"t\":\"log\",\"l\":\"$1\",\"m\":\"$2\"}" >&2
}

data() {
  echo "{\"t\":\"d\",\"f\":\"$1\",\"v\":\"$2\"}"
}

result() {
  echo "{\"t\":\"result\",\"ok\":$1,\"count\":$2}"
}

log "info" "Starting port scan"

TARGET="127.0.0.1"
PORTS=(22 80 443 3306 5432 8080)
FOUND=0

for PORT in "${PORTS[@]}"; do
  if timeout 1 bash -c "echo >/dev/tcp/$TARGET/$PORT" 2>/dev/null; then
    data "port" "$PORT"
    ((FOUND++))
  fi
done

result true $FOUND
```

### Batch Mode Example (Python)

```python
#!/usr/bin/env python3
# Name: Mass Domain Generator
# Description: Generates domains from wordlist
# Install: 
# InstallScope: global

import json
import sys

print(json.dumps({"bmop":"1.0","module":"domain-gen"}))

domains = []
with open('wordlist.txt') as f:
  for line in f:
    word = line.strip()
    domains.append(f"{word}.com")

BATCH_SIZE = 1000
for i in range(0, len(domains), BATCH_SIZE):
  batch = domains[i:i+BATCH_SIZE]
  
  print(json.dumps({"t":"batch","f":"domain","c":len(batch)}))
  for domain in batch:
    print(domain)
  print(json.dumps({"t":"batch_end"}))

print(json.dumps({"t":"result","ok":True,"count":len(domains)}))
```

---

## Best Practices

### 1. Use Stderr for Non-Data Output

All logs, progress, and status messages should go to stderr:

**Node.js:**
```javascript
console.error(JSON.stringify({t:"log",l:"info",m:"Processing..."}));
console.log(JSON.stringify({t:"d",f:"domain",v:"example.com"}));
```

**Python:**
```python
print(json.dumps({...}), file=sys.stderr)  # Logs
print(json.dumps({...}))                    # Data
```

### 2. Always Include Protocol Header

Start every module with:
```json
{"bmop":"1.0","module":"your-module-name"}
```

### 3. Always Send Result Message

End with success or failure:
```json
{"t":"result","ok":true,"count":42}
```

### 4. Use Batch Mode for Large Datasets

Switch to batch mode when outputting 100+ items:
```json
{"t":"batch","f":"domain","c":1000}
domain1.com
domain2.com
...
{"t":"batch_end"}
```

### 5. Handle Errors Gracefully

Always catch exceptions and report them:
```python
try:
  # Your code
  result(True, count=items)
except Exception as e:
  log("error", str(e))
  result(False, error=str(e))
  sys.exit(1)
```

### 6. Provide Progress Updates

For long operations, send progress every 50-100 items:
```json
{"t":"progress","c":150,"T":500}
```

---

## Module Examples

### Complete Example: Multi-Source Domain Collector

```javascript
#!/usr/bin/env node
// Name: Domain Collector
// Description: Collects domains from multiple bug bounty platforms
// Install: npm install axios
// InstallScope: shared

import axios from 'axios';

console.log(JSON.stringify({bmop:"1.0",module:"domain-collector"}));

const sources = [
  {name:"GitHub",url:"https://raw.githubusercontent.com/..."},
  {name:"Chaos",url:"https://chaos-data.projectdiscovery.io/..."}
];

async function collect() {
  const allDomains = new Set();
  
  console.error(JSON.stringify({t:"log",l:"info",m:"Starting collection"}));
  
  for (let i = 0; i < sources.length; i++) {
    const source = sources[i];
    console.error(JSON.stringify({t:"progress",c:i,T:sources.length,m:source.name}));
    
    try {
      const {data} = await axios.get(source.url);
      const domains = data.split('\n').filter(d => d.trim());
      
      domains.forEach(d => allDomains.add(d));
      console.error(JSON.stringify({t:"log",l:"info",m:`${source.name}: ${domains.length} domains`}));
      
    } catch (err) {
      console.error(JSON.stringify({t:"error",code:"FETCH",m:`${source.name} failed: ${err.message}`}));
    }
  }
  
  console.error(JSON.stringify({t:"log",l:"info",m:"Outputting results"}));
  
  const domainsArray = Array.from(allDomains);
  console.log(JSON.stringify({t:"batch",f:"domain",c:domainsArray.length}));
  domainsArray.forEach(d => console.log(d));
  console.log(JSON.stringify({t:"batch_end"}));
  
  console.log(JSON.stringify({t:"result",ok:true,count:allDomains.size}));
}

collect().catch(err => {
  console.error(JSON.stringify({t:"error",code:"FATAL",m:err.message,fatal:true}));
  console.log(JSON.stringify({t:"result",ok:false,error:err.message}));
  process.exit(1);
});
```

---

## Advanced Topics

### Custom Data Formats

You can define custom formats for specialized data:

```json
{"t":"d","f":"vulnerability","v":{"type":"XSS","severity":"high","url":"https://..."}}
{"t":"d","f":"credential","v":{"user":"admin","pass":"password123","service":"ftp"}}
{"t":"d","f":"certificate","v":{"domain":"example.com","expires":"2025-12-31"}}
```

### Metadata in Data Objects

Add metadata to provide context:

```json
{"t":"d","f":"domain","v":"example.com","meta":{"source":"chaos","confidence":0.95}}
{"t":"d","f":"subdomain","v":"api.example.com","meta":{"dns":"A","ip":"1.2.3.4"}}
```

### Performance Tips

1. **Buffer output** - Collect items in memory, output in batches
2. **Use batch mode** - 10x faster for large datasets
3. **Minimize JSON overhead** - Use short keys (`t`, `f`, `v`)
4. **Stream processing** - Don't load everything in memory

---

## Testing Your Module

### Manual Test
```bash
./bahamut run yourmodule.js > output.txt 2> logs.txt
```

### Check BMOP Compliance
```bash
./bahamut run your-module.js 2>/dev/null | head -1
# Should output: {"bmop":"1.0",...}
```

### Parse Output
```bash
./bahamut run your-module.js 2>/dev/null | \
  grep '"t":"d"' | \
  jq -r '.v'
```

---

## Future Enhancements

Bahamut is designed to be extended. Future capabilities may include:

- Module chaining (pipe output between modules)
- Parallel module execution
- Result aggregation and deduplication
- Built-in caching layer
- Module marketplace
- Real-time dashboards

---

## Contributing

When creating modules, follow these guidelines:

1. Use BMOP protocol correctly
2. Handle errors gracefully
3. Provide meaningful progress updates
4. Document your module with Name and Description
5. Specify accurate Install commands
6. Choose appropriate InstallScope
7. Test with various inputs
8. Keep dependencies minimal

---

## Support

For questions or issues:
- GitHub Issues: https://github.com/stringmanolo/bahamut/issues
- Documentation: docs/DOCS.md
- Examples: modules/ directory
