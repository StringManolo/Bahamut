import { jest, describe, it, expect, beforeEach, afterEach } from '@jest/globals';
import { showHelp } from "../../../src/cli/ui.ts";

describe("CLI UI", () => {
  let consoleSpy: jest.SpyInstance;
  const mockColor: any = new Proxy(() => mockColor, {
    get: () => mockColor,
    apply: (_target, _thisArg, args) => args[0]
  });


  beforeEach(() => {
    consoleSpy = jest.spyOn(console, 'log').mockImplementation();
  })

  afterEach(() => {
    consoleSpy.mockRestore();
  });

  describe("Help Menu", () => {
   it("Should list available commands", () => {
     showHelp(mockColor);
     expect(consoleSpy).toHaveBeenCalledWith(expect.stringMatching(/start.*stop/is));
   })

  })
/*
  describe("Command: Start", () => {
    it("Should display a message when the engine starts", () => {

    })
  })
*/
})
