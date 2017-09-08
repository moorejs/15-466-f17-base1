"use strict";

const fs = require("fs");
const path = require("path");
const util = require("util");
const exec = util.promisify(require("child_process").exec);

const directory = path.resolve(process.cwd(), process.argv[2] || ".");

console.log(`Listening for changes to "${directory}"`);

const onChange = {
  ".xcf": ({ filename, name, fullpath }) => {
    console.log(`Transforming ${filename} to png...`);

    exec(`convert -flatten ${fullpath} ${path.resolve(directory, name)}.png`)
      .then(({ stdout, stderr }) => {
        if (stderr) {
          console.error(stderr);
        }

        console.log(`Finished.\n`);
      })
      .catch(err => console.error(err));
  }
};

// TODO: allow options, such as verbose mode that prints commands

console.log(
  `Currently performing operations on the following filetypes: ${Object.keys(onChange).join(",")}`
);

console.log("Press Control + C to exit.\n");

const watcher = fs.watch(directory, (eventType, filename) => {
  if (!filename) {
    console.error("No filename provided...");
  }

  const file = {
    filename,
    extension: filename.slice(filename.lastIndexOf(".")), // including .
    name: filename.slice(0, filename.lastIndexOf(".")), // without extension
    fullpath: path.resolve(directory, filename)
  };

  switch (eventType) {
    case "change":
      if (typeof onChange[file.extension] === "function") {
        onChange[file.extension](file);
      }

      break;

    case "rename":
      // file could be removed or added

      break;
  }
});

process.once("SIGINT", () => {
  watcher.close();
});
