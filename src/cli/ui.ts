export const showHelp = (c: any): void => {
  console.log(`
  ${c.brightMagenta.bold("Bahamut")}

  ${c.bold.cyan("•")} ${c.bold.yellow("Start")}     ${c.dim.white("start scanner service")}
  ${c.bold.cyan("•")} ${c.bold.yellow("Stop")}      ${c.dim.white("stop scanner service")}

  `);
}
