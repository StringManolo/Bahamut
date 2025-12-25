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

Bahamut supports modules written in multiple languages: **JavaScript (Node.js)**, **Python**, and **Bash**.

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
# Install: sudo apt-get install curl jq
```

#### InstallScope

Defines where dependencies are installed:

- **`shared`** - Dependencies shared across all modules (recommended for common packages)
- **`isolated`** - Dependencies only for this module (use for conflicting versions)
- **`global`** - System-wide installation (use sparingly)

```javascript
// InstallScope: shared
```

#### Type (Optional)

Categorizes the module for automatic execution ordering:

- **`collector-*`** - Gathers data from external sources (e.g., `collector-domain`, `collector-subdomain`)
- **`processor`** - Transforms or enriches existing data
- **`output`** - Formats and exports final results

```javascript
// Type: collector-domain
```

#### Stage (Optional)

Defines execution order (lower numbers execute first):

```javascript
// Stage: 1    // Collectors (gather data)
// Stage: 2    // Processors (transform data)
// Stage: 3    // Outputs (export results)
```

#### Consumes (Optional)

Declares what data format this module needs as input:

```javascript
// Consumes: domain        // Needs domains as input
// Consumes: subdomain     // Needs subdomains as input
// Consumes: *             // Needs all available data
```

When `Consumes` is declared, core.cpp will pipe matching data to the module's stdin in BMOP format.

#### Provides (Optional)

Declares what data format this module produces:

```javascript
// Provides: subdomain     // Produces subdomains
// Provides: url           // Produces URLs
```

---

## Module Execution System

### Automatic Execution (Stage-Based)

Bahamut automatically executes modules in order based on their `Stage` declaration:

**Stage 1: Collectors** - Gather raw data
```javascript
// Type: collector-domain
// Stage: 1
// Provides: domain
```

**Stage 2: Processors** - Transform and enrich data
```javascript
// Type: processor
// Stage: 2
// Consumes: domain
// Provides: subdomain, url
```

**Stage 3: Outputs** - Export formatted results
```javascript
// Type: output
// Stage: 3
// Consumes: *
```

### Data Flow & Storage

Core.cpp maintains an in-memory storage system that accumulates data from all modules:

```cpp
storage["domain"] = ["example.com", "test.com", ...]
storage["subdomain"] = ["api.example.com", "www.test.com", ...]
storage["url"] = ["https://example.com", ...]
```

### Piping Modes

**Mode 1: Stdin Pipeline** (when `Consumes` is declared)
- Core.cpp pipes matching data format via stdin
- Module reads BMOP messages from stdin
- Module outputs new data via stdout

**Mode 2: Standalone** (no `Consumes` declaration)
- Module generates data independently
- Module outputs via stdout

### Execution Example

Given these modules:

**Module 1:** `getdomains.py`
```python
# Type: collector-domain
# Stage: 1
# Provides: domain
```

**Module 2:** `cleanwildcards.js`
```javascript
// Type: processor
// Stage: 2
// Consumes: domain
// Provides: domain
```

**Module 3:** `createsubdomains.py`
```python
# Type: processor
# Stage: 2
# Consumes: domain
# Provides: subdomain
```

**Module 4:** `tocsv.sh`
```bash
# Type: output
# Stage: 3
# Consumes: *
```

**Execution flow:**
```bash
./bahamut run all
```

1. **Stage 1:** Execute `getdomains.py` → Store in `storage["domain"]`
2. **Stage 2:** 
   - Pipe `storage["domain"]` to `cleanwildcards.js` → Update `storage["domain"]`
   - Pipe `storage["domain"]` to `createsubdomains.py` → Store in `storage["subdomain"]`
3. **Stage 3:** Pipe all storage to `tocsv.sh` → Generate output file

---

## Execution Profiles

Profiles allow you to define custom module execution sequences for specific workflows.

### Creating a Profile

Create a file in `profiles/bahamut_PROFILENAME.txt`:

**Example:** `profiles/bahamut_subdomains.txt`
```
checktor.js
getbugbountydomains.py
cleanwildcards.js
createsubdomains.py
exportcsv.sh
```

### Using Profiles

```bash
./bahamut run --profile subdomains
```

This executes only the modules listed in `profiles/bahamut_subdomains.txt` in the specified order.

### Profile Rules

- One module filename per line
- Modules execute in file order (ignoring Stage declarations)
- Comments allowed with `#`:
  ```
  # Data collection
  getdomains.py
  
  # Processing
  cleanwildcards.js
  createsubdomains.py
  ```
- Empty lines are ignored
- Module must exist in `modules/` directory tree

### Example Profiles

**`profiles/bahamut_recon.txt`** - Full reconnaissance
```
checktor.js
getbugbountydomains.py
getsubdomainsfromdns.py
createsubdomains.py
portscanner.py
exportjson.py
```

**`profiles/bahamut_quick.txt`** - Quick domain collection
```
getbugbountydomains.py
cleanwildcards.js
tocsv.sh
```

**`profiles/bahamut_process.txt`** - Only processing (assumes data exists)
```
cleanwildcards.js
addprotocols.js
deduplicatedomains.py
```

### Profile vs Auto-Execution

| Command | Behavior |
|---------|----------|
| `./bahamut run all` | Auto-execute all modules by Stage order |
| `./bahamut run --profile NAME` | Execute only modules in profile, in profile order |
| `./bahamut run module.py` | Execute single module |

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

### Collector Module (Stage 1)

Gathers data from external sources:

```javascript
#!/usr/bin/env node
// Name: Domain Scanner
// Description: Scans for domains from multiple sources
// Type: collector-domain
// Stage: 1
// Provides: domain
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

### Processor Module (Stage 2)

Consumes and transforms data:

```javascript
#!/usr/bin/env node
// Name: Subdomain Creator
// Description: Generates subdomains from domains
// Type: processor
// Stage: 2
// Consumes: domain
// Provides: subdomain
// Install: 
// InstallScope: global

console.log(JSON.stringify({bmop:"1.0",module:"subdomain-creator"}));

const subdomainPrefixes = ['www', 'api', 'mail', 'ftp', 'admin', 'dev'];
let processed = 0;

// Read from stdin (piped by core.cpp)
let buffer = '';
process.stdin.on('data', chunk => {
  buffer += chunk.toString();
  const lines = buffer.split('\n');
  buffer = lines.pop(); // Keep incomplete line in buffer
  
  lines.forEach(line => {
    if (!line.trim()) return;
    
    try {
      const msg = JSON.parse(line);
      
      // Process only domain data
      if (msg.t === 'd' && msg.f === 'domain') {
        const domain = msg.v;
        
        // Generate subdomains
        subdomainPrefixes.forEach(prefix => {
          const subdomain = `${prefix}.${domain}`;
          console.log(JSON.stringify({t:"d",f:"subdomain",v:subdomain}));
          processed++;
        });
      }
    } catch (e) {
      console.error(JSON.stringify({t:"error",code:"PARSE",m:e.message}));
    }
  });
});

process.stdin.on('end', () => {
  console.log(JSON.stringify({t:"result",ok:true,count:processed}));
});
```

### Output Module (Stage 3)

Exports formatted results:

```python
#!/usr/bin/env python3
# Name: CSV Exporter
# Description: Exports all data to CSV format
# Type: output
# Stage: 3
# Consumes: *
# Install: 
# InstallScope: global

import json
import sys
import csv

print(json.dumps({"bmop":"1.0","module":"csv-exporter"}))

data_by_format = {}

# Read all data from stdin
for line in sys.stdin:
  line = line.strip()
  if not line:
    continue
  
  try:
    msg = json.loads(line)
    
    if msg.get('t') == 'd':
      format_type = msg.get('f')
      value = msg.get('v')
      
      if format_type not in data_by_format:
        data_by_format[format_type] = []
      data_by_format[format_type].append(value)
      
  except json.JSONDecodeError:
    pass

# Export to CSV files
total = 0
for format_type, values in data_by_format.items():
  filename = f"output_{format_type}.csv"
  
  with open(filename, 'w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow([format_type])
    
    for value in values:
      writer.writerow([value])
      total += 1
  
  print(json.dumps({"t":"log","l":"info","m":f"Wrote {len(values)} items to {filename}"}), file=sys.stderr)

print(json.dumps({"t":"result","ok":True,"count":total}))
```

### Processor Module (Python)

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

### Batch Mode Example

For maximum performance with large datasets:

```python
#!/usr/bin/env python3
# Name: Mass Domain Generator
# Description: Generates domains from wordlist
# Type: collector-domain
# Stage: 1
# Provides: domain
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

## Creating Custom Profiles

### Step-by-Step Guide

**1. Create profile directory** (if it doesn't exist):
```bash
mkdir -p profiles
```

**2. Create profile file:**
```bash
touch profiles/bahamut_myworkflow.txt
```

**3. Add modules to profile:**

Edit `profiles/bahamut_myworkflow.txt`:
```
# My Custom Workflow
# Step 1: Check prerequisites
checktor.js

# Step 2: Collect data
getbugbountydomains.py
getsubdomainsfromdns.py

# Step 3: Process data
cleanwildcards.js
deduplicatedomains.py
createsubdomains.py

# Step 4: Export
exportcsv.sh
exportjson.py
```

**4. Run your profile:**
```bash
./bahamut run --profile myworkflow
```

### Profile Examples

**Basic Reconnaissance:**
```bash
# profiles/bahamut_recon.txt
getbugbountydomains.py
getsubdomainsfromdns.py
portscanner.py
screenshotter.js
exporthtml.sh
```

**Domain Processing Only:**
```bash
# profiles/bahamut_clean.txt
cleanwildcards.js
deduplicatedomains.py
sortdomains.sh
exportcsv.sh
```

**Quick Export:**
```bash
# profiles/bahamut_export.txt
exportcsv.sh
exportjson.py
exporthtml.sh
```

### Advanced Profile Usage

**Variables in profiles** (future feature):
```bash
# profiles/bahamut_targeted.txt
# TARGET=example.com
getsubdomains.py
portscan.py
```

**Conditional execution** (future feature):
```bash
# profiles/bahamut_smart.txt
getdomains.py
[if:tor_available] gettordomains.py
cleanwildcards.js
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

### 4. Declare Type and Stage

For automatic execution ordering:
```javascript
// Type: processor
// Stage: 2
// Consumes: domain
// Provides: subdomain
```

### 5. Handle Stdin When Using Consumes

If your module declares `Consumes`, read from stdin:

**Node.js:**
```javascript
let buffer = '';
process.stdin.on('data', chunk => {
  buffer += chunk.toString();
  const lines = buffer.split('\n');
  buffer = lines.pop();
  
  lines.forEach(line => {
    if (!line.trim()) return;
    const msg = JSON.parse(line);
    if (msg.t === 'd' && msg.f === 'domain') {
      // Process domain
    }
  });
});

process.stdin.on('end', () => {
  console.log(JSON.stringify({t:"result",ok:true,count:processed}));
});
```

**Python:**
```python
for line in sys.stdin:
  line = line.strip()
  if not line:
    continue
  msg = json.loads(line)
  if msg.get('t') == 'd' and msg.get('f') == 'domain':
    # Process domain
```

### 6. Use Batch Mode for Large Datasets

Switch to batch mode when outputting 1000+ items:
```json
{"t":"batch","f":"domain","c":1000}
domain1.com
domain2.com
...
{"t":"batch_end"}
```

### 7. Handle Errors Gracefully

Always catch exceptions and report them:
```python
try:
  # Your code
  print(json.dumps({"t":"result","ok":True,"count":items}))
except Exception as e:
  print(json.dumps({"t":"error","code":"FATAL","m":str(e)}), file=sys.stderr)
  print(json.dumps({"t":"result","ok":False,"error":str(e)}))
  sys.exit(1)
```

### 8. Provide Progress Updates

For long operations, send progress every 50-100 items:
```json
{"t":"progress","c":150,"T":500}
```

### 9. Use Appropriate Stages

- **Stage 1:** Data collection (collectors)
- **Stage 2:** Data transformation (processors)
- **Stage 3:** Output/export (outputs)
- **Stage 0:** Prerequisites/checks

### 10. Create Profiles for Workflows

Save common workflows as profiles:
```bash
# profiles/bahamut_daily.txt
checktor.js
getdomains.py
cleandata.js
export.sh
```

---

## Command Reference

| Command | Description |
|---------|-------------|
| `./bahamut run all` | Execute all modules by Stage order |
| `./bahamut run --profile NAME` | Execute modules from profile |
| `./bahamut run module.py` | Execute single module |
| `./bahamut install module.py` | Install module dependencies |
| `./bahamut uninstall module.py` | Remove module dependencies |
| `./bahamut list` | List all available modules |
| `./bahamut purge` | Remove all shared dependencies |

---

## Module Examples

### Complete Multi-Stage Workflow

This example shows a complete reconnaissance workflow with data flowing through stages:

**Stage 1: Collection**

`modules/collectors/getdomains.py`:
```python
#!/usr/bin/env python3
# Name: Bug Bounty Domain Collector
# Description: Collects domains from multiple bug bounty platforms
# Type: collector-domain
# Stage: 1
# Provides: domain
# Install: pip install requests beautifulsoup4
# InstallScope: shared

import json
import sys
import requests

print(json.dumps({"bmop":"1.0","module":"domain-collector"}))

sources = [
  "https://raw.githubusercontent.com/arkadiyt/bounty-targets-data/main/data/domains.txt"
]

domains = set()

for url in sources:
  try:
    r = requests.get(url, timeout=15)
    for line in r.text.split('\n'):
      domain = line.strip()
      if domain and not domain.startswith('#'):
        domains.add(domain)
  except Exception as e:
    print(json.dumps({"t":"error","code":"FETCH","m":str(e)}), file=sys.stderr)

for domain in sorted(domains):
  print(json.dumps({"t":"d","f":"domain","v":domain}))

print(json.dumps({"t":"result","ok":True,"count":len(domains)}))
```

**Stage 2: Processing**

`modules/processors/cleanwildcards.js`:
```javascript
#!/usr/bin/env node
// Name: Wildcard Domain Cleaner
// Description: Removes wildcard prefixes and deduplicates
// Type: processor
// Stage: 2
// Consumes: domain
// Provides: domain
// Install:
// InstallScope: global

console.log(JSON.stringify({bmop:"1.0",module:"wildcard-cleaner"}));

const domains = new Set();
let buffer = '';

process.stdin.on('data', chunk => {
  buffer += chunk.toString();
  const lines = buffer.split('\n');
  buffer = lines.pop();
  
  lines.forEach(line => {
    if (!line.trim()) return;
    
    try {
      const msg = JSON.parse(line);
      if (msg.t === 'd' && msg.f === 'domain') {
        let domain = msg.v;
        
        // Remove wildcard
        if (domain.startsWith('*.')) {
          domain = domain.substring(2);
        }
        
        // Deduplicate
        domains.add(domain);
      }
    } catch (e) {}
  });
});

process.stdin.on('end', () => {
  // Output cleaned domains
  Array.from(domains).sort().forEach(domain => {
    console.log(JSON.stringify({t:"d",f:"domain",v:domain}));
  });
  
  console.log(JSON.stringify({t:"result",ok:true,count:domains.size}));
});
```

`modules/processors/generatesubdomains.py`:
```python
#!/usr/bin/env python3
# Name: Subdomain Generator
# Description: Generates common subdomains from domains
# Type: processor
# Stage: 2
# Consumes: domain
# Provides: subdomain
# Install:
# InstallScope: global

import json
import sys

print(json.dumps({"bmop":"1.0","module":"subdomain-generator"}))

prefixes = ['www', 'api', 'mail', 'ftp', 'admin', 'dev', 'staging', 'test']
count = 0

for line in sys.stdin:
  line = line.strip()
  if not line:
    continue
  
  try:
    msg = json.loads(line)
    if msg.get('t') == 'd' and msg.get('f') == 'domain':
      domain = msg.get('v')
      
      for prefix in prefixes:
        subdomain = f"{prefix}.{domain}"
        print(json.dumps({"t":"d","f":"subdomain","v":subdomain}))
        count += 1
        
  except:
    pass

print(json.dumps({"t":"result","ok":True,"count":count}))
```

**Stage 3: Output**

`modules/outputs/exportcsv.sh`:
```bash
#!/usr/bin/env bash
# Name: CSV Exporter
# Description: Exports all data to CSV files
# Type: output
# Stage: 3
# Consumes: *
# Install:
# InstallScope: global

echo '{"bmop":"1.0","module":"csv-exporter"}'

# Create output files
> domains.csv
> subdomains.csv

echo "domain" > domains.csv
echo "subdomain" > subdomains.csv

DOMAIN_COUNT=0
SUBDOMAIN_COUNT=0

while IFS= read -r line; do
  [[ -z "$line" ]] && continue
  
  # Extract format and value
  if echo "$line" | grep -q '"t":"d"'; then
    FORMAT=$(echo "$line" | sed -n 's/.*"f":"\([^"]*\)".*/\1/p')
    VALUE=$(echo "$line" | sed -n 's/.*"v":"\([^"]*\)".*/\1/p')
    
    case "$FORMAT" in
      "domain")
        echo "$VALUE" >> domains.csv
        ((DOMAIN_COUNT++))
        ;;
      "subdomain")
        echo "$VALUE" >> subdomains.csv
        ((SUBDOMAIN_COUNT++))
        ;;
    esac
  fi
done

TOTAL=$((DOMAIN_COUNT + SUBDOMAIN_COUNT))

echo "{\"t\":\"log\",\"l\":\"info\",\"m\":\"Exported $DOMAIN_COUNT domains to domains.csv\"}" >&2
echo "{\"t\":\"log\",\"l\":\"info\",\"m\":\"Exported $SUBDOMAIN_COUNT subdomains to subdomains.csv\"}" >&2
echo "{\"t\":\"result\",\"ok\":true,\"count\":$TOTAL}"
```

**Running the workflow:**

```bash
./bahamut run all
```

**What happens:**
1. Stage 1: `getdomains.py` collects 35,000 domains → stored in memory
2. Stage 2 (parallel):
   - `cleanwildcards.js` cleans wildcards from 35,000 domains → 34,500 clean domains
   - `generatesubdomains.py` creates 8 subdomains per domain → 276,000 subdomains
3. Stage 3: `exportcsv.sh` writes everything to CSV files

**Output files:**
- `domains.csv` - 34,500 clean domains
- `subdomains.csv` - 276,000 subdomains

### Using a Profile

Create `profiles/bahamut_recon.txt`:
```
getdomains.py
cleanwildcards.js
generatesubdomains.py
exportcsv.sh
```

Run with:
```bash
./bahamut run --profile recon
```

---

## Profile File Format

### Syntax Rules

1. **One module per line** - Module filename only
2. **Comments** - Lines starting with `#` are ignored
3. **Empty lines** - Ignored
4. **Module lookup** - Core.cpp searches in `modules/` directory tree
5. **Execution order** - Top to bottom, ignoring Stage declarations

### Example Profile

```
# My Reconnaissance Profile
# Author: YourName
# Purpose: Full domain reconnaissance

# Prerequisites
checktor.js

# Data Collection (parallel execution possible)
getbugbountydomains.py
getcrtshdomains.py

# Data Processing
cleanwildcards.js
deduplicatedomains.py

# Subdomain Generation
createsubdomains.py
verifysubdomains.js

# Export Results
exportcsv.sh
exportjson.py
exporthtml.sh
```

### Profile Naming Convention

- **Location:** `profiles/bahamut_NAME.txt`
- **Usage:** `./bahamut run --profile NAME`
- **Lowercase recommended:** `bahamut_quick_scan.txt`

### Profile Organization

Organize profiles by purpose:

```
profiles/
├── bahamut_recon.txt          # Full reconnaissance
├── bahamut_quick.txt          # Fast domain collection
├── bahamut_deep.txt           # Deep enumeration
├── bahamut_export_only.txt    # Export existing data
├── bahamut_process_only.txt   # Process without collection
└── bahamut_custom.txt         # Your custom workflow
```

---

## Data Format Reference

### Common Data Formats

| Format | Description | Example |
|--------|-------------|---------|
| `domain` | Root domain | `example.com` |
| `subdomain` | Subdomain | `api.example.com` |
| `url` | Full URL | `https://example.com/path` |
| `ip` | IP address | `192.168.1.1` |
| `port` | Port number | `443` |
| `email` | Email address | `admin@example.com` |
| `hash` | Hash value | `5d41402abc4b2a76b9719d911017c592` |
| `vulnerability` | Security finding | `{"type":"XSS","severity":"high"}` |
| `certificate` | SSL certificate | `{"cn":"example.com","expires":"2025-12-31"}` |

### Creating Custom Formats

Define your own formats for specialized data:

```javascript
console.log(JSON.stringify({
  t: "d",
  f: "custom-finding",
  v: {
    type: "misconfiguration",
    severity: "medium",
    details: "CORS wildcard enabled"
  }
}));
```

---

## Performance Optimization

### For Module Developers

**1. Use Batch Mode for Large Outputs**
```python
# Bad: Individual messages for 100k items
for domain in domains:
  print(json.dumps({"t":"d","f":"domain","v":domain}))

# Good: Batch mode
print(json.dumps({"t":"batch","f":"domain","c":len(domains)}))
for domain in domains:
  print(domain)
print(json.dumps({"t":"batch_end"}))
```

**2. Buffer Stdin Reads**
```javascript
// Bad: Process each character
process.stdin.on('data', chunk => {
  chunk.toString().split('').forEach(char => { ... });
});

// Good: Buffer complete lines
let buffer = '';
process.stdin.on('data', chunk => {
  buffer += chunk.toString();
  const lines = buffer.split('\n');
  buffer = lines.pop();
  lines.forEach(processLine);
});
```

**3. Minimize JSON Overhead**
```python
# Bad: Verbose keys
print(json.dumps({"type":"data","format":"domain","value":"example.com"}))

# Good: Short keys
print(json.dumps({"t":"d","f":"domain","v":"example.com"}))
```

**4. Stream Processing**
```python
# Bad: Load everything in memory
all_data = []
for line in sys.stdin:
  all_data.append(process(line))
for item in all_data:
  output(item)

# Good: Stream processing
for line in sys.stdin:
  output(process(line))
```

### For Core.cpp

- Use memory mapping for large datasets
- Implement disk overflow when memory exceeds threshold
- Parallel execution within same stage
- Connection pooling for network modules

---

## Troubleshooting

### Module Not Found

```
[-] Error: Module mymodule.py not found.
```

**Solution:** Ensure module exists in `modules/` directory tree.

### Dependencies Not Installing

```
[-] Installation failed with exit code: 256
```

**Solution:** Check Install command syntax and InstallScope.

### Module Not Receiving Data

**Check:**
1. Module declares `Consumes: format`
2. Previous stage produces that format
3. Stage ordering is correct
4. Module reads from stdin

### BMOP Parse Errors

```
[-] Failed to parse: ...
```

**Solution:** Ensure valid JSON on each line, no syntax errors.

### Profile Not Working

```
[-] Profile not found: myprofile
```

**Solution:** 
- File must be `profiles/bahamut_myprofile.txt`
- Use command: `./bahamut run --profile myprofile` (without .txt)

---

## Advanced Topics

### Module Chaining

Modules automatically chain when:
- Stage N produces format X
- Stage N+1 consumes format X

Example:
```
Stage 1: collector-domain → produces "domain"
Stage 2: processor (consumes "domain") → produces "subdomain"
Stage 3: output (consumes "subdomain") → exports
```

### Parallel Execution

Modules in the same stage can run in parallel (future feature):

```
Stage 2:
  - cleanwildcards.js (consumes: domain, provides: domain)
  - generatesubdomains.py (consumes: domain, provides: subdomain)
  - portscan.js (consumes: domain, provides: port)
```

All three receive the same input simultaneously.

### Data Persistence

Core.cpp storage is temporary (in-memory). To persist data:

**Option 1:** Use output modules
```javascript
// Type: output
// Stage: 99
// Saves to disk for next run
```

**Option 2:** External storage
```python
import redis
redis_client.set('domains', json.dumps(domains))
```

### Custom Stage Numbers

Use custom stages for fine-grained control:

```
Stage 0: Prerequisites (check dependencies)
Stage 1: Primary collectors
Stage 2: Secondary collectors (use stage 1 data)
Stage 10: Processing
Stage 20: Enrichment
Stage 90: Pre-export validation
Stage 99: Export
Stage 100: Cleanup
```

---

## Future Enhancements

Planned features for Bahamut:

### Core Features
- Parallel module execution within stages
- Disk overflow for large datasets
- Query API for runtime data access
- Module dependencies (DAG resolution)
- Real-time progress dashboard
- Module marketplace/repository

### Profile Features
- Variables in profiles (`$TARGET`, `$WORDLIST`)
- Conditional execution (`[if:condition]`)
- Profile inheritance (`@include other_profile`)
- Loop constructs (`[for:domain in domains]`)

### BMOP Extensions
- Binary protocol option for performance
- Compression support (gzip inline)
- Checksums for data integrity
- Streaming encryption
- Multi-format output (JSON, CSV, XML in one)

### Module Features
- Hot reload without restart
- Sandboxed execution
- Resource limits (CPU, memory, time)
- Module versioning
- Automatic updates

---

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
./bahamut run your-module.js > output.txt 2> logs.txt
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

### Module Requirements

1. **Valid shebang** - Specify correct interpreter
2. **Complete headers** - Name, Description, Install, InstallScope
3. **BMOP compliance** - Use protocol correctly
4. **Type and Stage** - Declare for automatic execution
5. **Consumes/Provides** - Declare for data flow
6. **Error handling** - Graceful failure with error messages
7. **Progress updates** - For long-running operations
8. **Result message** - Always send final result

### Code Quality

- **Minimal dependencies** - Keep install lightweight
- **Clean code** - Readable and maintainable
- **Comments** - Explain complex logic
- **Testing** - Test with various inputs
- **Performance** - Use batch mode for large datasets

### Documentation

- **Clear name** - Descriptive module name
- **Detailed description** - Explain what it does
- **Examples** - Show usage in comments
- **Data formats** - Document custom formats

### Submission

1. Place module in appropriate directory:
   ```
   modules/collectors/    # For data gathering
   modules/processors/    # For data transformation
   modules/outputs/       # For data export
   ```

2. Create example profile:
   ```
   profiles/bahamut_yourmodule.txt
   ```

3. Test thoroughly:
   ```bash
   ./bahamut run yourmodule.py
   ./bahamut run --profile yourmodule
   ```

4. Submit pull request with:
   - Module file
   - Example profile
   - Documentation updates

---

## Support

For questions or issues:

- **GitHub Issues:** https://github.com/stringmanolo/bahamut/issues
- **Documentation:** docs/DOCS.md  
- **Examples:** modules/ directory
- **Profiles:** profiles/ directory

## Quick Reference Card

### Module Headers
```javascript
#!/usr/bin/env node
// Name: Your Module Name
// Description: What it does
// Type: collector-domain | processor | output
// Stage: 1 | 2 | 3
// Consumes: domain | subdomain | * (optional)
// Provides: domain | subdomain | url (optional)
// Install: npm install package
// InstallScope: shared | isolated | global
```

### BMOP Messages
```javascript
// Protocol init
{bmop:"1.0",module:"name"}

// Log
{t:"log",l:"info|warn|error|debug",m:"message"}

// Progress
{t:"progress",c:current,T:total}

// Data
{t:"d",f:"format",v:"value"}

// Batch
{t:"batch",f:"format",c:count}
value1
value2
{t:"batch_end"}

// Result
{t:"result",ok:true|false,count:N}

// Error
{t:"error",code:"CODE",m:"message"}
```

### Commands
```bash
./bahamut run all                    # Run all by stage
./bahamut run --profile NAME         # Run profile
./bahamut run module.py              # Run single module
./bahamut install module.py          # Install dependencies
./bahamut list                       # List modules
```

### Profile Format
```
# profiles/bahamut_name.txt
# Comments start with #

module1.py
module2.js
module3.sh
```

---

**Happy hacking with Bahamut! 🐉**