#!/usr/bin/env node

"use strict";

const fs = require("fs");
const path = require("path");
const { exec } = require("child_process");

const cwd = process.cwd();
const directories = process.argv.slice(2).map(filepath => path.resolve(cwd, filepath));

if (!directories.length) {
  directories.push(path.resolve(cwd, "."));
}

console.log(`Listening for changes to:\n${directories.join("\n")}\n`);

let proc;

const onChange = {
  ".xcf": ({ filename, name, fullpath, directory }) => {
    console.log(`Transforming ${filename} to png...`);

    exec(
      `convert -flatten ${fullpath} ${path.resolve(directory, name)}.png`,
      (err, stdout, stderr) => {
        if (err) {
          console.error(err);
        }

        if (stderr) {
          console.error(stderr);
          return;
        }

        if (stdout) {
          console.log(stdout);
        }

        console.log(`Finished.`);

        if (proc) {
          onChange[".cpp"]();
        }
      }
    );
  },
  ".cpp": () => {
    console.log("Jamming file...");
    exec("jam", (err, stdout, stderr) => {
      if (proc) {
        console.log("Killing previous executable...\n");
        proc.kill();
      }

      // stderr vs err, not much different?
      if (stderr) {
        console.error(stderr);
        return;
      }

      if (stdout) {
        console.log(stdout);
      }

      console.log("Running executable...");
      proc = exec("./dist/main");
      proc.stdout.on("data", data => process.stdout.write(`main-stdout: ${data}`));
      proc.stderr.on("data", err =>
        process.stderr.write(`main-stderr: \x1b[4m\x1b[31m${err}\x1b[0m`)
      );
    });
  }
};

// TODO: allow options, such as verbose mode that prints commands

console.log(
  `Currently performing operations on the following filetypes: ${Object.keys(onChange).join(", ")}`
);

console.log("Press Control + C to exit.\n");

const watcherCallback = directory => (eventType, filename) => {
  if (!filename) {
    console.error("No filename provided...");
  }

  const file = {
    filename,
    directory,
    extension: filename.slice(filename.lastIndexOf(".")), // including .
    name: filename.slice(0, filename.lastIndexOf(".")), // without extension
    fullpath: path.resolve(directory, filename)
  };

  switch (eventType) {
    case "change":
      if (typeof onChange[file.extension] === "function") {
        console.log(`\n${filename} changed. Processing...\n`);
        onChange[file.extension](file);
      }

      break;

    case "rename":
      // file could be removed or added

      break;
  }
};

const watchers = directories.map(directory => fs.watch(directory, watcherCallback(directory)));

process.once("SIGINT", () => {
  console.log("\nClosing up shop...");

  watchers.forEach(watcher => watcher.close());

  if (proc) {
    proc.kill();
  }
});
