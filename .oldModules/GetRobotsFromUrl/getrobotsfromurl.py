#!/usr/bin/env python3
# Name: Robots.txt Fetcher
# Description: Fetches and outputs robots.txt from target URL
# Type: collector-url
# Stage: 1
# Provides: url
# Args: -u, --url <url> (required) Target URL to fetch robots.txt from
# Args: -v, --verbose (flag, optional) Show detailed information
# Args: --version (flag, optional) Show module version
# Install: pip install requests
# InstallScope: shared

import argparse
import json
import sys
import requests
from urllib.parse import urljoin, urlparse

VERSION = "1.0.0"

def log(level, message):
  print(json.dumps({"t": "log", "l": level, "m": message}), file=sys.stderr)

def data(format_type, value):
  print(json.dumps({"t": "d", "f": format_type, "v": value}))

def result(success, **kwargs):
  print(json.dumps({"t": "result", "ok": success, **kwargs}))

def main():
  parser = argparse.ArgumentParser(description="Fetch robots.txt from URL")
  parser.add_argument("-u", "--url", required=True, help="Target URL")
  parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
  parser.add_argument("--version", action="store_true", help="Show version")
  
  args = parser.parse_args()

  print(json.dumps({
    "bmop": "1.0",
    "module": "robots-fetcher",
    "version": VERSION,
    "args": vars(args)
  }))

  if args.version:
    log("info", f"Robots.txt Fetcher v{VERSION}")
    result(True, version=VERSION)
    sys.exit(0)

  url = args.url
  if not url.startswith(("http://", "https://")):
    url = "https://" + url

  parsed = urlparse(url)
  robots_url = f"{parsed.scheme}://{parsed.netloc}/robots.txt"

  if args.verbose:
    log("info", f"Target URL: {url}")
    log("info", f"Robots URL: {robots_url}")

  try:
    response = requests.get(robots_url, timeout=10, allow_redirects=True)
    
    if response.status_code == 200:
      content = response.text
      lines = content.strip().split('\n')
      line_count = len(lines)
      
      if args.verbose:
        log("info", f"HTTP Status: {response.status_code}")
        log("info", f"Content-Type: {response.headers.get('Content-Type', 'N/A')}")
        log("info", f"Content Length: {len(content)} bytes")
        log("info", f"Total Lines: {line_count}")
        log("info", f"Response Time: {response.elapsed.total_seconds():.2f}s")
      
      for line in lines:
        stripped = line.strip()
        if stripped and not stripped.startswith('#'):
          if stripped.lower().startswith(('disallow:', 'allow:', 'sitemap:')):
            parts = stripped.split(':', 1)
            if len(parts) == 2:
              path = parts[1].strip()
              if path and path != '/':
                if path.startswith('http'):
                  data("url", path)
                else:
                  full_url = urljoin(robots_url, path)
                  data("url", full_url)
      
      result(True, lines=line_count, status=response.status_code)
      
    elif response.status_code == 404:
      log("warn", "No robots.txt found (404)")
      result(True, lines=0, status=404, message="No robots.txt")
      
    else:
      log("error", f"Unexpected status code: {response.status_code}")
      result(False, status=response.status_code, error="Unexpected status")
      sys.exit(1)
      
  except requests.exceptions.Timeout:
    log("error", "Request timeout")
    result(False, error="Timeout")
    sys.exit(1)
    
  except requests.exceptions.ConnectionError:
    log("error", "Connection failed")
    result(False, error="Connection error")
    sys.exit(1)
    
  except Exception as e:
    log("error", f"Unexpected error: {str(e)}")
    result(False, error=str(e))
    sys.exit(1)

if __name__ == "__main__":
  main()
