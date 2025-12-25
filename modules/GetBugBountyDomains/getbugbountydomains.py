#!/usr/bin/env python3
# Name: Get Bug Bounty Domains
# Description: Extracts bug bounty target domains from multiple public sources (Chaos, GitHub lists, etc.)
# Install: pip install requests beautifulsoup4
# InstallScope: shared
import requests
import sys
import json
from bs4 import BeautifulSoup
from concurrent.futures import ThreadPoolExecutor, as_completed
import zipfile
import io

session = requests.Session()
session.headers.update({
  "User-Agent": "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36"
})

def fetch_chaos_program(program_name):
  domains = set()
  data_url = f"https://chaos-data.projectdiscovery.io/{program_name}.zip"
  
  try:
    response = session.get(data_url, timeout=10, stream=True)
    if response.status_code == 200:
      with zipfile.ZipFile(io.BytesIO(response.content)) as z:
        for filename in z.namelist():
          with z.open(filename) as f:
            for line in f:
              domain = line.decode('utf-8').strip()
              if domain and '.' in domain:
                root = '.'.join(domain.split('.')[-2:])
                domains.add(root)
  except:
    pass
  
  return domains

def get_chaos_domains():
  all_domains = set()
  print("[*] Fetching from Chaos (ProjectDiscovery)...", file=sys.stderr)
  
  try:
    url = "https://chaos-data.projectdiscovery.io/index.json"
    response = session.get(url, timeout=15)
    
    if response.status_code == 200:
      programs = response.json()
      total = len(programs)
      print(f"[*] Found {total} programs in Chaos", file=sys.stderr)
      print(f"[*] Processing with 20 parallel workers...", file=sys.stderr)
      
      program_names = [p.get("name", "") for p in programs if p.get("name")]
      
      with ThreadPoolExecutor(max_workers=20) as executor:
        futures = {executor.submit(fetch_chaos_program, name): name for name in program_names}
        
        completed = 0
        for future in as_completed(futures):
          completed += 1
          domains = future.result()
          all_domains.update(domains)
          
          if completed % 50 == 0 or completed == total:
            print(f"[*] Progress: {completed}/{total} programs processed ({len(all_domains)} domains)", file=sys.stderr)
      
      print(f"[+] Chaos: {len(all_domains)} domains", file=sys.stderr)
    else:
      print(f"[-] Chaos returned status {response.status_code}", file=sys.stderr)
      
  except Exception as e:
    print(f"[-] Error fetching Chaos: {e}", file=sys.stderr)
  
  return all_domains

def get_github_bounty_targets():
  domains = set()
  print("[*] Fetching from GitHub bounty-targets...", file=sys.stderr)
  
  try:
    url = "https://raw.githubusercontent.com/arkadiyt/bounty-targets-data/main/data/domains.txt"
    response = session.get(url, timeout=15)
    
    if response.status_code == 200:
      lines = response.text.strip().split('\n')
      for line in lines:
        domain = line.strip()
        if domain and not domain.startswith('#'):
          domains.add(domain)
      
      print(f"[+] GitHub: {len(domains)} domains", file=sys.stderr)
    else:
      print(f"[-] GitHub returned status {response.status_code}", file=sys.stderr)
      
  except Exception as e:
    print(f"[-] Error fetching GitHub list: {e}", file=sys.stderr)
  
  return domains

def get_intigriti_programs():
  domains = set()
  print("[*] Fetching from Intigriti...", file=sys.stderr)
  
  try:
    url = "https://www.intigriti.com/programs"
    response = session.get(url, timeout=15)
    
    if response.status_code == 200:
      soup = BeautifulSoup(response.text, 'html.parser')
      scripts = soup.find_all('script', type='application/json')
      
      for script in scripts:
        try:
          data = json.loads(script.string)
          if isinstance(data, dict) and "props" in data:
            programs = data.get("props", {}).get("pageProps", {}).get("programs", [])
            for program in programs:
              scopes = program.get("scopes", [])
              for scope in scopes:
                endpoint = scope.get("endpoint", "")
                if endpoint:
                  endpoint = endpoint.replace("*.", "").replace("https://", "").replace("http://", "").split("/")[0]
                  if endpoint:
                    domains.add(endpoint)
        except:
          continue
      
      print(f"[+] Intigriti: {len(domains)} domains", file=sys.stderr)
    else:
      print(f"[-] Intigriti returned status {response.status_code}", file=sys.stderr)
      
  except Exception as e:
    print(f"[-] Error fetching Intigriti: {e}", file=sys.stderr)
  
  return domains

def get_yeswehack_programs():
  domains = set()
  print("[*] Fetching from YesWeHack...", file=sys.stderr)
  
  try:
    url = "https://api.yeswehack.com/programs"
    response = session.get(url, timeout=15)
    
    if response.status_code == 200:
      data = response.json()
      programs = data.get("items", []) if isinstance(data, dict) else data
      
      for program in programs:
        if not isinstance(program, dict):
          continue
          
        if program.get("public", False):
          scopes = program.get("scopes", [])
          for scope in scopes:
            if not isinstance(scope, dict):
              continue
              
            scope_type = scope.get("scope_type", "")
            if "web" in str(scope_type).lower() or "url" in str(scope_type).lower():
              target = scope.get("scope", "")
              if target and isinstance(target, str):
                target = target.replace("*.", "").replace("https://", "").replace("http://", "").split("/")[0]
                if target:
                  domains.add(target)
      
      print(f"[+] YesWeHack: {len(domains)} domains", file=sys.stderr)
    else:
      print(f"[-] YesWeHack returned status {response.status_code}", file=sys.stderr)
      
  except Exception as e:
    print(f"[-] Error fetching YesWeHack: {e}", file=sys.stderr)
  
  return domains

def get_hackerone_disclosed():
  domains = set()
  print("[*] Fetching from HackerOne disclosed reports...", file=sys.stderr)
  
  try:
    url = "https://hackerone.com/hacktivity.json"
    response = session.get(url, timeout=15)
    
    if response.status_code == 200:
      data = response.json()
      reports = data.get("reports", [])
      
      for report in reports:
        team = report.get("team", {})
        handle = team.get("handle", "")
        profile = team.get("profile", {})
        website = profile.get("website", "")
        
        if website:
          domain = website.replace("https://", "").replace("http://", "").replace("www.", "").split("/")[0]
          if domain:
            domains.add(domain)
      
      print(f"[+] HackerOne: {len(domains)} domains", file=sys.stderr)
    else:
      print(f"[-] HackerOne returned status {response.status_code}", file=sys.stderr)
      
  except Exception as e:
    print(f"[-] Error fetching HackerOne: {e}", file=sys.stderr)
  
  return domains

def get_bugcrowd_programs():
  domains = set()
  print("[*] Fetching from Bugcrowd...", file=sys.stderr)
  
  try:
    url = "https://bugcrowd.com/programs"
    response = session.get(url, timeout=15)
    
    if response.status_code == 200:
      soup = BeautifulSoup(response.text, 'html.parser')
      
      scripts = soup.find_all('script')
      for script in scripts:
        if script.string and 'window.__INITIAL_STATE__' in script.string:
          try:
            json_str = script.string.split('window.__INITIAL_STATE__ = ')[1].split(';</script>')[0]
            data = json.loads(json_str)
            programs = data.get("programs", {}).get("list", [])
            
            for program in programs:
              targets = program.get("targets", [])
              for target in targets:
                if target.get("category") == "website":
                  domain = target.get("name", "")
                  if domain:
                    domain = domain.replace("*.", "").replace("https://", "").replace("http://", "").split("/")[0]
                    if domain:
                      domains.add(domain)
          except:
            continue
      
      print(f"[+] Bugcrowd: {len(domains)} domains", file=sys.stderr)
    else:
      print(f"[-] Bugcrowd returned status {response.status_code}", file=sys.stderr)
      
  except Exception as e:
    print(f"[-] Error fetching Bugcrowd: {e}", file=sys.stderr)
  
  return domains

def main():
  all_domains = set()
  
  print("[*] Gathering bug bounty domains from multiple sources...", file=sys.stderr)
  print("=" * 60, file=sys.stderr)
  
  sources = [
    ("GitHub", get_github_bounty_targets),
    ("Chaos", get_chaos_domains),
    ("HackerOne", get_hackerone_disclosed),
    ("Intigriti", get_intigriti_programs),
    ("YesWeHack", get_yeswehack_programs),
    ("Bugcrowd", get_bugcrowd_programs)
  ]
  
  with ThreadPoolExecutor(max_workers=6) as executor:
    futures = {executor.submit(func): name for name, func in sources}
    
    for future in as_completed(futures):
      source_name = futures[future]
      try:
        domains = future.result()
        all_domains.update(domains)
        print(f"[+] {source_name} completed - Total unique: {len(all_domains)}", file=sys.stderr)
      except Exception as e:
        print(f"[-] {source_name} failed: {e}", file=sys.stderr)
  
  print("=" * 60, file=sys.stderr)
  print(f"[+] Total unique domains collected: {len(all_domains)}", file=sys.stderr)
  
  if not all_domains:
    print("[-] No domains found from any source", file=sys.stderr)
    sys.exit(1)
  
  for domain in sorted(all_domains):
    if domain:
      print(domain)

if __name__ == "__main__":
  try:
    main()
  except KeyboardInterrupt:
    print("\n[!] Interrupted by user", file=sys.stderr)
    sys.exit(130)
  except Exception as e:
    print(f"[-] Fatal error: {e}", file=sys.stderr)
    sys.exit(1)
