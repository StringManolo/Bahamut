#!/usr/bin/env node

const { execSync } = require('child_process');

const message = process.argv[2];
const skipHooks = process.argv.includes('--no-verify');

if (!message) {
  console.error('Error: Commit message missing.');
  process.exit(1);
}

try {
  execSync('git add --all', { stdio: 'inherit' });
  
  const commitCommand = skipHooks 
    ? `git commit -m "${message}" --no-verify` 
    : `git commit -m "${message}"`;
    
  execSync(commitCommand, { stdio: 'inherit' });
  execSync('git push', { stdio: 'inherit' });
} catch (error) {
  process.exit(1);
}
