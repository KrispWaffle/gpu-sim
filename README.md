# Simulating GPU
More of an NVIDIA GPU though


# Syntax
The program is just a vector of type `Instr` 
```c++
using Operand = std::variant<Opcode, std::string, float, Variable, StoreLoc>;
struct Instr
{
    Opcode op;
    std::vector<Operand> src;
};
```
so just add into the vector what operations etc you want to do and insert it into the gpu and run
```c++
GPU gpu(program);
   
gpu.run();
```

It goes 
- Operation 
    - Destination
    - Source
    - Value (if required)

Once you have your program add a halt to let GPU know its the end

When you want your destination to be thread indexed you just put `TIDX` at the end of the destination 

EX 
```c++
{Opcode::ADD, {"smTIDX", "x", "z"}}
```
This stores in shared memory thread indexed.

If you want to create a variable you do 

```c++
ISCONSTANT = false;
THREADINDEXED = true;
Variable{"name", value, index, ISCONSTANT, THREADINDEXED, StoreLoc::WHEREVER}
```
if you set THREADINDEXED to true it will cancel out the index value so you can just set it to zero.

There are 3 store locations 

`StoreLoc::GLOBAL`
`StoreLoc::LOCAL`
`StoreLoc::SHARED`

GLOBAL means it will be stored in global memory

LOCAL means it will stored in register

SHARED stores in warp/shared memory
# Instructions
- ADD
- SUB
- MUL
- NEG 
- LD (load)
- ST (store)
- MOV 
- HALT (end of program)
- DEF (create variables)


# Extra
You can print Global and Shared memory by using `print_global_mem` and `print_shared_mem` on your gpu object
```c++
gpu.print_global_mem();
gpu.print_shared_mem();
```