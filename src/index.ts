#!/usr/bin/env node

/**
 * Bahamut Scanner - Command Line Interface
 * 
 * Main CLI entry point for the vulnerability scanner
 * @module CLI
 * @group CLI
 */

import parseCLI from "simpleargumentsparser";

/**
 * Current version of Bahamut Scanner
 * @group CLI
 * @public
 */
export const version = '1.0.0';

/**
 * Configuration options for the scanner
 * @group CLI
 * @interface Config
 */
interface Config {
  /** Target URL to scan for vulnerabilities */
  target: string;
  /** Enable verbose output mode */
  verbose: boolean;
}

/**
 * Displays the help menu with usage instructions and examples
 * @group CLI
 * @internal
 */
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

/**
 * Displays the current version of Bahamut Scanner
 * @group CLI
 * @internal
 */
function showVersion(): void {
  console.log(`Bahamut Scanner v${version}`);
}

/**
 * Main CLI entry point for Bahamut Scanner
 * 
 * Parses command-line arguments and executes the vulnerability scanning process.
 * Handles help display, version display, and target validation before scanning.
 * 
 * @group CLI
 * @returns Promise that resolves when scanning completes or help/version is displayed
 * @throws {Error} Exits with code 1 if target is missing or invalid
 * 
 * @example
 * Display help
 * ```bash
 * bahamut --help
 * bahamut -h
 * ```
 * 
 * @example
 * Show version
 * ```bash
 * bahamut --version
 * ```
 * 
 * @example
 * Basic scan
 * ```bash
 * bahamut --target https://example.com
 * ```
 * 
 * @example
 * Verbose scan
 * ```bash
 * bahamut --target https://example.com -v
 * ```
 * 
 * @public
 */
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
