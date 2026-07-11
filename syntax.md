# Current syntax
## Variable declaration
```
var name: type = value;
```
Value is optional. Type is optional if value is present.
## Constant declaration
```
const name: type = value;
```
Type is optional.
## Type declaration
```
type Name = Type;
```
## Function declaration
```
fn name(param1: type, param2: type) -> return_type {
	// function body
}

fn no_return_type() {
}

// Named return type has no special meaning
fn named_return_type() -> (result: int) {
}
```
## Struct
```
type Struct = struct {
    field: type;
};
```
## Array and Slice
```
var array: [100]int;
var element: int = array[44];
var slice: []int = array[:];
var sub_slice = slice[10:90] // Range of elements is [10, 90)
var first_half = slice[:50];
var second_half = slice[50:];
```
## Pointer
```
var b: int;
var a: *int = &b;
var q: int = a.*;
```
## If
```
if expression {
	// if body
} else if {
	// if body
} else {
	// else body
}
```
## While
```
while expression {
	// loop body
}
```
## Struct literals
```
var v = Vec2{1, 1};
var v2: Vec2 = {.x=-1,.y=1};
// Not allowed
// var v3 = Vec2{0, .y=0};
```
## Casts
```
var f: f32;
var i = cast(s32)f;
```
## Function pointer
```
type Fn = fn() -> int;
```
## Nested declarations
Declarations can appear at any scope, except for anonymous structs.
```
type T = struct {
	type IntPtr = *int;
	
	fn f() {
		g();
		
		fn g() {
		}
	}
	
	var v: int;
	const X = 1;
};
var p: T.IntPtr;

fn main() {
	f();
	var a: struct {
		// Not allowed
		// const Y = 2;	
	};
}
```