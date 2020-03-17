export function loadAndIncrement(location: i32): i32 {
  let val = load<i32>(location as i32);
  val += 1;
  return val;
}
