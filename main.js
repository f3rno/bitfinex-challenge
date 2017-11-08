const addon = require('./build/Release/addon');

const input = [];
const output = [];

addon.startInputThread();

const process = () => {
  addon.flushPendingInputs(input);

  // Did we receive anything?
  if (input.length > 0) {
    for (let i = 0; i < input.length; i += 1) {
      output.push(input[i] ** 2);
    }

    addon.flushToOutput(output);

    // Empty it for next time, or make it local to this scope...
    input.splice(0, input.length);
    output.splice(0, output.length);
  }

  setImmediate(process);
};

process();
