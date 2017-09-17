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
  },
  ".info": ({ fullpath, directory, name }) => {
    const { Transform } = require("stream");

    fs.readFile(fullpath, (err, data) => {
      let input = data.toString();
      input = input.split(/\n/).map(line => {
        // test: (0.0, 0.4),2.0 ==> 0.0, 0.4,2.0 ==> [0.0, 0.4, 2.0]
        const stripped = line.replace(/\(|\)|.*: /g, "");
        return stripped.split(/, ?/);
      });

      const HEADER_BYTES = 8;
      const FLOAT_BYTES = 4;
      const FLOATS_PER_LINE = 6;
      const out = Buffer.alloc(HEADER_BYTES + FLOAT_BYTES * FLOATS_PER_LINE * input.length);

      let offset = HEADER_BYTES;
      input.forEach(row =>
        row.forEach(val => {
          if (/.*\..*/.test(val)) {
            out.writeFloatLE(val, offset);
          } else {
            if (/.*w$/.test(val)) {
              console.log("writing " + val + " to " + val.slice(0, val.length - 1) / 320);
              out.writeFloatLE(val.slice(0, val.length - 1) / 320, offset);
            } else {
              console.log("writing " + val + " to " + (1-(val / 240)));
              out.writeFloatLE(1-(val / 240), offset);
            }
          }
          offset += FLOAT_BYTES;
        })
      );

      out.writeUInt32LE(offset - HEADER_BYTES, 0);

      fs.writeFile(path.resolve(directory, name) + ".file", out, "binary", err => {
        console.log("Done\n");
      });
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
