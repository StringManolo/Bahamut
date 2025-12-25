#!/usr/bin/env python3
# Name: Get Bug Bounty Domains
# Description: Extracts bug bounty target domains from multiple public sources (Chaos, GitHub lists, etc.) - Demo of python module
# Type: collector-domain
# Stage: 1
# Provides: domain
# Install: pip install requests beautifulsoup4
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

def fetch_chaos_program(program_name):
  domains = set()
  data_url = f"https://chaos-data.projectdiscovery.io/{program_name}.zip"

  try:
    response = session.get(data_url, timeout=10, stream=True)
    if response.status_code == 200 and len(response.content) > 0:
      with zipfile.ZipFile(io.BytesIO(response.content)) as z:
        for filename in z.namelist():
          with z.open(filename) as f:
            for line in f:
              try:
                domain = line.decode('utf-8').strip()
                if domain and '.' in domain and not domain.startswith('#'):
                  parts = domain.split('.')
                  if len(parts) >= 2:
                    root = '.'.join(parts[-2:])
                    if root and len(root) > 3:
                      domains.add(root)
              except:
                continue
  except:
    pass

  return domains

def get_chaos_domains():
  all_domains = set()
  print(json.dumps({"t":"log","l":"info","m":"Fetching from Chaos (ProjectDiscovery)..."}), file=sys.stderr)

  try:
    url = "https://chaos-data.projectdiscovery.io/index.json"
    response = session.get(url, timeout=15)

    if response.status_code == 200:
      programs = response.json()
      total = len(programs)
      print(json.dumps({"t":"log","l":"info","m":f"Found {total} programs in Chaos"}), file=sys.stderr)
      print(json.dumps({"t":"log","l":"info","m":"Processing with 20 parallel workers..."}), file=sys.stderr)

      program_names = [p.get("name", "") for p in programs if p.get("name")]

      with ThreadPoolExecutor(max_workers=20) as executor:
        futures = {executor.submit(fetch_chaos_program, name): name for name in program_names}

        completed = 0
        for future in as_completed(futures):
          completed += 1
          domains = future.result()
          all_domains.update(domains)

          if completed % 50 == 0 or completed == total:
            print(json.dumps({"t":"progress","c":completed,"T":total,"m":f"{len(all_domains)} domains"}), file=sys.stderr)

      print(json.dumps({"t":"log","l":"info","m":f"Chaos: {len(all_domains)} domains"}), file=sys.stderr)
    else:
      print(json.dumps({"t":"log","l":"warn","m":f"Chaos returned status {response.status_code}"}), file=sys.stderr)

  except Exception as e:
    print(json.dumps({"t":"error","code":"CHAOS","m":str(e)}), file=sys.stderr)

  return all_domains

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

def get_intigriti_programs():
  domains = set()
  print(json.dumps({"t":"log","l":"info","m":"Fetching from Intigriti..."}), file=sys.stderr)

  try:
    url = "https://api.intigriti.com/external/researcher/v1/programs"
    response = session.get(url, timeout=15)

    if response.status_code == 200:
      try:
        programs = response.json()
        for program in programs:
          if isinstance(program, dict):
            domains_list = program.get("domains", [])
            for domain_obj in domains_list:
              if isinstance(domain_obj, dict):
                domain = domain_obj.get("endpoint", "")
              elif isinstance(domain_obj, str):
                domain = domain_obj
              else:
                continue

              if domain:
                domain = domain.replace("*.", "").replace("https://", "").replace("http://", "").split("/")[0].split(":")[0]
                if domain and '.' in domain:
                  domains.add(domain)

        print(json.dumps({"t":"log","l":"info","m":f"Intigriti: {len(domains)} domains"}), file=sys.stderr)
      except:
        print(json.dumps({"t":"log","l":"warn","m":"Intigriti JSON parse error"}), file=sys.stderr)
    else:
      print(json.dumps({"t":"log","l":"warn","m":f"Intigriti returned status {response.status_code}"}), file=sys.stderr)

  except Exception as e:
    print(json.dumps({"t":"error","code":"INTIGRITI","m":str(e)}), file=sys.stderr)

  return domains

def get_yeswehack_programs():
  domains = set()
  print(json.dumps({"t":"log","l":"info","m":"Fetching from YesWeHack..."}), file=sys.stderr)

  try:
    url = "https://api.yeswehack.com/programs"
    response = session.get(url, timeout=15)

    if response.status_code == 200:
      try:
        data = response.json()
        programs = data if isinstance(data, list) else data.get("items", [])

        for program in programs:
          if not isinstance(program, dict):
            continue

          scopes = program.get("scopes", [])
          if isinstance(scopes, list):
            for scope in scopes:
              if not isinstance(scope, dict):
                continue

              target = scope.get("scope", "")
              if target and isinstance(target, str):
                target = target.replace("*.", "").replace("https://", "").replace("http://", "").split("/")[0].split(":")[0]
                if target and '.' in target:
                  domains.add(target)

        print(json.dumps({"t":"log","l":"info","m":f"YesWeHack: {len(domains)} domains"}), file=sys.stderr)
      except:
        print(json.dumps({"t":"log","l":"warn","m":"YesWeHack JSON parse error"}), file=sys.stderr)
    else:
      print(json.dumps({"t":"log","l":"warn","m":f"YesWeHack returned status {response.status_code}"}), file=sys.stderr)

  except Exception as e:
    print(json.dumps({"t":"error","code":"YESWEHACK","m":str(e)}), file=sys.stderr)

  return domains

def get_hackerone_disclosed():
  domains = set()
  print(json.dumps({"t":"log","l":"info","m":"Fetching from HackerOne disclosed reports..."}), file=sys.stderr)

  try:
    url = "https://hackerone.com/hacktivity.json?querystring=&sort_type=latest_disclosable_activity_at&filter=type%3Apublic"
    response = session.get(url, timeout=15)

    if response.status_code == 200:
      try:
        data = response.json()
        reports = data.get("reports", [])

        for report in reports:
          team = report.get("team", {})
          profile = team.get("profile", {})
          website = profile.get("website", "")

          if website:
            domain = website.replace("https://", "").replace("http://", "").replace("www.", "").split("/")[0].split(":")[0]
            if domain and '.' in domain:
              domains.add(domain)

        print(json.dumps({"t":"log","l":"info","m":f"HackerOne: {len(domains)} domains"}), file=sys.stderr)
      except json.JSONDecodeError:
        print(json.dumps({"t":"log","l":"warn","m":"HackerOne returned non-JSON (rate limited or blocked)"}), file=sys.stderr)
    else:
      print(json.dumps({"t":"log","l":"warn","m":f"HackerOne returned status {response.status_code}"}), file=sys.stderr)

  except Exception as e:
    print(json.dumps({"t":"error","code":"HACKERONE","m":str(e)}), file=sys.stderr)

  return domains

def get_bugcrowd_programs():
  domains = set()
  print(json.dumps({"t":"log","l":"info","m":"Fetching from Bugcrowd..."}), file=sys.stderr)

  try:
    url = "https://raw.githubusercontent.com/arkadiyt/bounty-targets-data/main/data/bugcrowd_data.json"
    response = session.get(url, timeout=15)

    if response.status_code == 200:
      try:
        programs = response.json()
        for program in programs:
          targets = program.get("targets", {}).get("in_scope", [])
          for target in targets:
            if target.get("type") in ["website", "api"]:
              domain = target.get("target", "")
              if domain:
                domain = domain.replace("*.", "").replace("https://", "").replace("http://", "").split("/")[0].split(":")[0]
                if domain and '.' in domain:
                  domains.add(domain)

        print(json.dumps({"t":"log","l":"info","m":f"Bugcrowd: {len(domains)} domains"}), file=sys.stderr)
      except:
        print(json.dumps({"t":"log","l":"warn","m":"Bugcrowd JSON parse error"}), file=sys.stderr)
    else:
      print(json.dumps({"t":"log","l":"warn","m":f"Bugcrowd returned status {response.status_code}"}), file=sys.stderr)

  except Exception as e:
    print(json.dumps({"t":"error","code":"BUGCROWD","m":str(e)}), file=sys.stderr)

  return domains

def get_hackerone_programs():
  domains = set()
  print(json.dumps({"t":"log","l":"info","m":"Fetching from HackerOne programs data..."}), file=sys.stderr)

  try:
    url = "https://raw.githubusercontent.com/arkadiyt/bounty-targets-data/main/data/hackerone_data.json"
    response = session.get(url, timeout=15)

    if response.status_code == 200:
      try:
        programs = response.json()
        for program in programs:
          targets = program.get("targets", {}).get("in_scope", [])
          for target in targets:
            if target.get("type") in ["URL", "WILDCARD"]:
              domain = target.get("instruction", "") or target.get("asset_identifier", "")
              if domain:
                domain = domain.replace("*.", "").replace("https://", "").replace("http://", "").split("/")[0].split(":")[0]
                if domain and '.' in domain:
                  domains.add(domain)

        print(json.dumps({"t":"log","l":"info","m":f"HackerOne Programs: {len(domains)} domains"}), file=sys.stderr)
      except:
        print(json.dumps({"t":"log","l":"warn","m":"HackerOne Programs JSON parse error"}), file=sys.stderr)
    else:
      print(json.dumps({"t":"log","l":"warn","m":f"HackerOne Programs returned status {response.status_code}"}), file=sys.stderr)

  except Exception as e:
    print(json.dumps({"t":"error","code":"HACKERONE_PROGRAMS","m":str(e)}), file=sys.stderr)

  return domains

def main():
  all_domains = set()

  print(json.dumps({"t":"log","l":"info","m":"Gathering bug bounty domains from multiple sources..."}), file=sys.stderr)

  sources = [
    ("GitHub Domains", get_github_bounty_targets),
    ("Chaos", get_chaos_domains),
    ("HackerOne Programs", get_hackerone_programs),
    ("HackerOne Disclosed", get_hackerone_disclosed),
    ("Bugcrowd", get_bugcrowd_programs),
    ("Intigriti", get_intigriti_programs),
    ("YesWeHack", get_yeswehack_programs)
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
