#!/usr/bin/env node

import parseCLI from "simpleargumentsparser";

export const version = '1.0.0';

interface Config {
  target: string;
  verbose: boolean;
}

function showHelp(): void {
  console.log(`
Bahamut Help Menu
=================

Usage: bahamut [options]

Options:
  -h, --help              Show this help menu
  --version               Show version number
  --target <url>          Target URL to scan
  -v, --verbose           Enable verbose mode

Examples:
  bahamut --target https://example.com
  bahamut --target https://example.com -v
`);
}

function showVersion(): void {
  console.log(`Bahamut Scanner v${version}`);
}

export async function main(): Promise<void> {
  const cli = await parseCLI();
  
  // Check for help
  if (cli.s.h || cli.c.help || (cli.o.length > 0 && cli.o[0][0] === 'help') || cli.noArgs) {
    showHelp();
    return;
  }

  // Check for version
  if (cli.c.version) {
    showVersion();
    return;
  }

  const config: Config = {
    target: cli.c.target as string || "",
    verbose: !!(cli.s.v || cli.c.verbose)
  };

  console.log(`Bahamut Scanner v${version}`);

  if (!config.target) {
    console.log(cli.color.red`Error: --target is required`);
    console.log(cli.color.dim`Use -h for help`);
    process.exit(1);
  }

  if (config.verbose) {
    console.log(cli.color.dim`Input: ${config.target}`);
  }

  // Scan target...
  console.log(cli.color.green("✓ Scan complete"));
}

// Execute only if run directly
if (import.meta.url === `file://${process.argv[1]}`) {
  main().catch((error) => {
    console.error('Fatal error:', error);
    process.exit(1);
  });
}
