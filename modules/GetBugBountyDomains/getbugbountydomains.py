#!/usr/bin/env python3
# Name: Get Bug Bounty Domains
# Description: Extracts bug bounty target domains from multiple public sources (Chaos, GitHub lists, etc.) - Demo of python module
# Type: collector-domain
# Stage: 1
# Provides: domain
# Install: pip install requests bs4
# InstallScope: shared
import requests
import sys
import json
import os
from bs4 import BeautifulSoup
from concurrent.futures import ThreadPoolExecutor, as_completed
import zipfile
import io

print(json.dumps({"bmop":"1.0","module":"domain-collector","pid":os.getpid()}))
session = requests.Session()
session.headers.update({
  "User-Agent": "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36"
})

def get_github_bounty_targets():
  domains = set()
  print(json.dumps({"t":"log","l":"info","m":"Fetching from GitHub bounty-targets..."}), file=sys.stderr)

  try:
    url = "https://raw.githubusercontent.com/arkadiyt/bounty-targets-data/main/data/domains.txt"
    response = session.get(url, timeout=15)
    if response.status_code == 200:
      lines = response.text.strip().split('\n')
      for line in lines:
        domain = line.strip()
        if domain and not domain.startswith('#'):
          domains.add(domain)
      print(json.dumps({"t":"log","l":"info","m":f"GitHub: {len(domains)} domains"}), file=sys.stderr)
    else:
      print(json.dumps({"t":"log","l":"warn","m":f"GitHub returned status {response.status_code}"}), file=sys.stderr)

  except Exception as e:
    print(json.dumps({"t":"error","code":"GITHUB","m":str(e)}), file=sys.stderr)

  return domains

def get_github_0dayan0n_bounty_targets():
  domains = set()
  print(json.dumps({"t":"log","l":"info","m":"Fetching from GitHub 0dayan0n bounty targets..."}), file=sys.stderr)

  try:
    url = "https://raw.githubusercontent.com/0dayan0n/Download-All-Bug-Bounty-Programs-Domains-In-Scope-/refs/heads/main/domains.txt"
    response = session.get(url, timeout=15)
    if response.status_code == 200:
      lines = response.text.strip().split('\n')
      for line in lines:
        domain = line.strip()
        if domain and not domain.startswith('#'):
          domains.add(domain)
      print(json.dumps({"t":"log","l":"info","m":f"GitHub 0dayan0n: {len(domains)} domains"}), file=sys.stderr)
    else:
      print(json.dumps({"t":"log","l":"warn","m":f"GitHub 0dayan0n returned status {response.status_code}"}), file=sys.stderr)

  except Exception as e:
    print(json.dumps({"t":"error","code":"GITHUB_0DAYAN0N","m":str(e)}), file=sys.stderr)

  return domains

def main():
  all_domains = set()

  print(json.dumps({"t":"log","l":"info","m":"Gathering bug bounty domains from multiple sources..."}), file=sys.stderr)

  sources = [
    ("GitHub Domains", get_github_bounty_targets),
    ("GitHub 0dayan0n Domains", get_github_0dayan0n_bounty_targets)
  ]

  with ThreadPoolExecutor(max_workers=7) as executor:
    futures = {executor.submit(func): name for name, func in sources}

    for future in as_completed(futures):
      source_name = futures[future]
      try:
        domains = future.result()
        all_domains.update(domains)
        print(json.dumps({"t":"log","l":"info","m":f"{source_name} completed - Total unique: {len(all_domains)}"}), file=sys.stderr)
      except Exception as e:
        print(json.dumps({"t":"error","code":"SOURCE","m":f"{source_name} failed: {str(e)}"}), file=sys.stderr)

  print(json.dumps({"t":"log","l":"info","m":f"Total unique domains collected: {len(all_domains)}"}), file=sys.stderr)

  if not all_domains:
    print(json.dumps({"t":"error","code":"NO_DOMAINS","m":"No domains found from any source"}), file=sys.stderr)
    print(json.dumps({"t":"result","ok":False,"error":"No domains found"}))
    sys.exit(1)

  sorted_domains = sorted(all_domains)

  print(json.dumps({"t":"batch","f":"domain","c":len(sorted_domains)}))
  for domain in sorted_domains:
    if domain:
      print(domain)
  print(json.dumps({"t":"batch_end"}))

  print(json.dumps({"t":"result","ok":True,"count":len(sorted_domains)}))

if __name__ == "__main__":
  try:
    main()
  except KeyboardInterrupt:
    print(json.dumps({"t":"error","code":"INTERRUPTED","m":"Interrupted by user"}), file=sys.stderr)
    print(json.dumps({"t":"result","ok":False,"error":"Interrupted"}))
    sys.exit(130)
  except Exception as e:
    print(json.dumps({"t":"error","code":"FATAL","m":str(e)}), file=sys.stderr)
    print(json.dumps({"t":"result","ok":False,"error":str(e)}))
    sys.exit(1)
