# utrace
An out-of-date tool for user space call flow tracing

-----
I want to introduce a new developed tool called "utrace"(linux + x86 version), which can be used for “User defined function calls TRACE”. It will benefit 5060 learning/debugging. 


As a common demonstration, if you already have a binary "test" which includes recursive function call like following:
int callMe(int n)
{
     if (n == 0)
     {
           return 0;
     }
     callMe(n-1);

     return n;
}
...

int main(int argc, void* argv[])

{
     ...
     while (1)

     {

           callMe(4);

         ...

     }
     ...
}


Suppose the "test" process is running with pid 5715, you can "utrace" the "test" process like following:

simon@lionteeth:~$ utrace -Cp 5715

...
[ 5715]  +[ 1]callMe(int)(4, 5715, 0xb7cce4c8, 0xb7f06341, 0)   <unfinished ...>
[ 5715]   +[ 2]callMe(int)(3, 0xb7ccebb0, 0xb7cce448, 0x8048a2d, 4)   <unfinished ...>
[ 5715]    +[ 3]callMe(int)(2, 0, 0xb7cce438, 0x804891d, 3)   <unfinished ...>
[ 5715]     +[ 4]callMe(int)(1, 0, 0xb7cce428, 0x804891d, 2)   <unfinished ...>
[ 5715]      +[ 5]callMe(int)(0, 0, 0xb7cce418, 0x804891d, 1)  = 0 
[ 5715]     -[ 4]callMe(int) resumed> = 1 
[ 5715]    -[ 3]callMe(int) resumed> = 2 
[ 5715]   -[ 2]callMe(int) resumed> = 3 
[ 5715]  -[ 1]callMe(int) resumed> = 4 
...

As can be seen, utrace will reveal the function call flow, arguments and return value in the fly, with completely no need to insert code or recompile "test" binary. 

As an explanation of the output: 

                [ 5715]      +[ 5]callMe(int)(0, 0, 0xb7cce418, 0x804891d, 1)  = 0 

- "[5715]" means the thread id. (main thread id equals process id)
- "+" means running into a function (if "-", it means escaping from a function).
- "[ 5]" means entering call stack depth 5.
- "callMe(int)(0, 0, 0xb7cce418, 0x804891d, 1)" means argument 1~5# is (0, 0, 0xb7cce418, 0x804891d, 1). Since callMe() only has 1 argument, the last 4 arguments make no sense(utrace just retrieve random values at stack).
- "  = 0" means return value of CallMe() is 0. 
 

Press Ctrl+c will detach utrace from the process you are tracing. 
 
A simple explanation for command "utrace -Cp 5715" options: 
   "-C" option means demangling C++ function name. 
   "-p" option is followed by the pid to be traced. 

 
Besides tracing "test" on the fly (dynamically attach to a running program with -p option), You can trace the "test" program from the very beginning with following command:
   utrace -C ./test


Utrace supports multithreading and works for 5060 processes in lincase. Take CpCallm as an example, a new CpCallm learner can easily acquire a basic call flow with following at CCM :
"utrace -Cp `pgrep -x CpCallm` "

    After making a basic call, part of the output can be following:
...

[14183]    +[ 3]CmnPrSdlInstance::processSignal(SdlSignal&, bool)(0xf5a537ac, 0xff80df78, 0, 100, 0x9f15d98)   <unfinished ...>
[14183]     +[ 4]CpCallmOhcp::ohcpSdl(CmnPrSdlProcess&, int, int, int, void*)(0xa02fe44, 109, 37, 17, 0x108fd6a0)   <unfinished ...>
[14183]      +[ 5]CpCallmTrioTrace::saveSignalHistory(int, int, int, int, direction_type)(0x1006e088, -7, 109, 17, 37)  = 24
[14183]      +[ 5]CpCallmHcp::saveStateHistory(CpCallmHcpState::CommonStates)(0xf1c8c898, 109, 109, 17, 37)  = 0
[14183]      +[ 5]CpCallmHcp::saveSignalHistory(int, int)(0xf1c8c898, 17, 37, 17, 37)  = 0
[14183]      +[ 5]CpCallmOhcpMobile::procOrigAll(CmnPrSdlProcess&, int, int, int, void*)(0xf1c8c898, 0xa02fe44, 109, 37, 17)  = -1
[14183]      +[ 5]CpCallmHcp::changeState2(CmnPrSdlProcess&, CpCallmHcpState::CommonStates, char*, int)(0xf1c8c898, 0xa02fe44, -1, 0x969e404, 300)   <unfinished ...>
....

   If the output rolls too quickly, -o option can be used for output redirection. Further advance usage of utrace can be explored with "utrace -h".
