import { describe, expect, it, jest, beforeEach, afterEach } from '@jest/globals';
import { main, version } from '../../src/index.js';

describe('Bahamut Scanner', () => {
  let consoleLogSpy: jest.SpiedFunction<typeof console.log>;
  let consoleErrorSpy: jest.SpiedFunction<typeof console.error>;
  let processExitSpy: jest.SpiedFunction<typeof process.exit>;
  let originalArgv: string[];

  beforeEach(() => {
    consoleLogSpy = jest.spyOn(console, 'log').mockImplementation();
    consoleErrorSpy = jest.spyOn(console, 'error').mockImplementation();
    processExitSpy = jest.spyOn(process, 'exit').mockImplementation(() => undefined as never);
    originalArgv = process.argv;
  });

  afterEach(() => {
    consoleLogSpy.mockRestore();
    consoleErrorSpy.mockRestore();
    processExitSpy.mockRestore();
    process.argv = originalArgv;
  });

  describe('Version', () => {
    it('should have correct version', () => {
      expect(version).toBe('1.0.0');
    });

    it('should display version with --version flag', async () => {
      process.argv = ['node', 'bahamut', '--version'];
      await main();
      expect(consoleLogSpy).toHaveBeenCalledWith(expect.stringContaining('1.0.0'));
    });
  });

  describe('Help Menu', () => {
    it('should show help with no arguments', async () => {
      process.argv = ['node', 'bahamut'];
      await main();
      expect(consoleLogSpy).toHaveBeenCalledWith(expect.stringContaining('Bahamut Help Menu'));
    });

    it('should show help with -h flag', async () => {
      process.argv = ['node', 'bahamut', '-h'];
      await main();
      expect(consoleLogSpy).toHaveBeenCalledWith(expect.stringContaining('Bahamut Help Menu'));
    });

    it('should show help with --help flag', async () => {
      process.argv = ['node', 'bahamut', '--help'];
      await main();
      expect(consoleLogSpy).toHaveBeenCalledWith(expect.stringContaining('Bahamut Help Menu'));
    });

    it('should show help with help argument', async () => {
      process.argv = ['node', 'bahamut', 'help'];
      await main();
      expect(consoleLogSpy).toHaveBeenCalledWith(expect.stringContaining('Bahamut Help Menu'));
    });
  });

  describe('Target Validation', () => {
    it('should exit with error when no target provided', async () => {
      process.argv = ['node', 'bahamut', '--scan'];
      await main();
      expect(processExitSpy).toHaveBeenCalledWith(1);
    });

    it('should accept valid target', async () => {
      process.argv = ['node', 'bahamut', '--target', 'https://example.com'];
      await main();
      expect(consoleLogSpy).toHaveBeenCalledWith(expect.stringContaining('Scan complete'));
    });
  });

  describe('Verbose Mode', () => {
    it('should show verbose output with -v flag', async () => {
      process.argv = ['node', 'bahamut', '--target', 'https://example.com', '-v'];
      await main();
      expect(consoleLogSpy).toHaveBeenCalledWith(expect.stringContaining('Target:'));
    });

    it('should show verbose output with --verbose flag', async () => {
      process.argv = ['node', 'bahamut', '--target', 'https://example.com', '--verbose'];
      await main();
      expect(consoleLogSpy).toHaveBeenCalledWith(expect.stringContaining('Target:'));
    });

    it('should not show verbose output without verbose flag', async () => {
      process.argv = ['node', 'bahamut', '--target', 'https://example.com'];
      await main();
      const calls = consoleLogSpy.mock.calls.map(call => call[0]);
      const hasTargetLog = calls.some(call => 
        typeof call === 'string' && call.includes('Target:')
      );
      expect(hasTargetLog).toBe(false);
    });
  });

  describe('Main Function', () => {
    it('should be exported and callable', () => {
      expect(typeof main).toBe('function');
    });

    it('should return a Promise', () => {
      process.argv = ['node', 'bahamut', '--help'];
      const result = main();
      expect(result).toBeInstanceOf(Promise);
    });
  });
});
