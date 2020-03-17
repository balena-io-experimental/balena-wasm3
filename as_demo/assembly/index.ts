import {
  Console,
  FileSystem,
  Descriptor
} from "../node_modules/as-wasi/assembly/as-wasi";

export function add(a: i32, b: i32): i32 {
  Console.log("Computing Addition");
  return a + b;
}

export function readFile(): i32 {
  let file = FileSystem.open("/usr/src/app/test.txt") as Descriptor;
  let contents = file.readString()!;
  Console.log(contents);
  return 0;
}

export function _start(): void {
  Console.log("Idling...");

  while (1) {}
}
