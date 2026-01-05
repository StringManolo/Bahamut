#!/usr/bin/env python3
# Name: Get HTTP Proxies
# Description: Extracts HTTP proxies from multiple public sources
# Type: collector-proxy
# Stage: 1
# Provides: httpproxy
# Install: pip install requests bs4 lxml
# InstallScope: shared

import requests
import sys
import json
import os
import re
from concurrent.futures import ThreadPoolExecutor, as_completed
import time

print(json.dumps({"bmop":"1.0","module":"proxy-collector","pid":os.getpid()}))
session = requests.Session()
session.headers.update({
  "User-Agent": "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36"
})

def validate_proxy(proxy_str):
  pattern = r'^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}:\d{1,5}$'
  if re.match(pattern, proxy_str):
    ip, port = proxy_str.split(':')
    octets = ip.split('.')
    if len(octets) == 4 and all(0 <= int(o) <= 255 for o in octets):
      if 1 <= int(port) <= 65535:
        return True
  return False

def get_proxyscrape_proxies():
  proxies = set()
  print(json.dumps({"t":"log","l":"info","m":"Fetching from ProxyScrape..."}), file=sys.stderr)
  try:
    urls = [
      "https://api.proxyscrape.com/v3/free-proxy-list/get?request=displayproxies&protocol=http&proxy_format=ipport&format=text&timeout=10000",
      "https://api.proxyscrape.com/v3/free-proxy-list/get?request=displayproxies&protocol=https&proxy_format=ipport&format=text&timeout=10000",
      "https://api.proxyscrape.com/v2/?request=getproxies&protocol=http&timeout=10000&country=all&ssl=all&anonymity=all"
    ]
    for url in urls:
      try:
        response = session.get(url, timeout=20)
        if response.status_code == 200:
          lines = response.text.strip().split('\n')
          for line in lines:
            proxy = line.strip()
            if proxy and validate_proxy(proxy):
              proxies.add(proxy)
        time.sleep(1)
      except:
        continue
    print(json.dumps({"t":"log","l":"info","m":f"ProxyScrape: {len(proxies)} proxies"}), file=sys.stderr)
  except Exception as e:
    print(json.dumps({"t":"error","code":"PROXYSCRAPE","m":str(e)}), file=sys.stderr)
  return proxies

def get_github_proxylist_proxies():
  proxies = set()
  print(json.dumps({"t":"log","l":"info","m":"Fetching from GitHub ProxyList..."}), file=sys.stderr)
  try:
    urls = [
      "https://raw.githubusercontent.com/TheSpeedX/PROXY-List/master/http.txt",
      "https://raw.githubusercontent.com/ShiftyTR/Proxy-List/master/http.txt",
      "https://raw.githubusercontent.com/roosterkid/openproxylist/main/HTTP_RAW.txt",
      "https://raw.githubusercontent.com/httpanime/Proxy-List/main/http.txt",
      "https://raw.githubusercontent.com/ProxyScraper/Proxy-List/main/http.txt"
    ]
    for url in urls:
      try:
        response = session.get(url, timeout=15)
        if response.status_code == 200:
          lines = response.text.strip().split('\n')
          for line in lines:
            proxy = line.strip()
            if proxy and validate_proxy(proxy):
              proxies.add(proxy)
        time.sleep(0.5)
      except:
        continue
    print(json.dumps({"t":"log","l":"info","m":f"GitHub ProxyList: {len(proxies)} proxies"}), file=sys.stderr)
  except Exception as e:
    print(json.dumps({"t":"error","code":"GITHUB_PROXYLIST","m":str(e)}), file=sys.stderr)
  return proxies

def main():
  all_proxies = set()
  print(json.dumps({"t":"log","l":"info","m":"Gathering HTTP proxies from multiple sources..."}), file=sys.stderr)

  sources = [
    ("GitHub ProxyList", get_github_proxylist_proxies),
    ("ProxyScrape", get_proxyscrape_proxies)
  ]

  with ThreadPoolExecutor(max_workers=2) as executor:
    futures = {executor.submit(func): name for name, func in sources}
    for future in as_completed(futures):
      source_name = futures[future]
      try:
        proxies = future.result()
        all_proxies.update(proxies)
        print(json.dumps({"t":"log","l":"info","m":f"{source_name} completed - Total unique: {len(all_proxies)}"}), file=sys.stderr)
      except Exception as e:
        print(json.dumps({"t":"error","code":"SOURCE","m":f"{source_name} failed: {str(e)}"}), file=sys.stderr)

  print(json.dumps({"t":"log","l":"info","m":f"Total unique proxies collected: {len(all_proxies)}"}), file=sys.stderr)

  if not all_proxies:
    print(json.dumps({"t":"error","code":"NO_PROXIES","m":"No proxies found from any source"}), file=sys.stderr)
    print(json.dumps({"t":"result","ok":False,"error":"No proxies found"}))
    sys.exit(1)

  sorted_proxies = sorted(all_proxies, key=lambda x: tuple(map(int, x.split(':')[0].split('.'))))

  print(json.dumps({"t":"batch","f":"httpproxy","c":len(sorted_proxies)}))
  for proxy in sorted_proxies:
    if proxy:
      print(proxy)
  print(json.dumps({"t":"batch_end"}))

  print(json.dumps({"t":"result","ok":True,"count":len(sorted_proxies)}))

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
