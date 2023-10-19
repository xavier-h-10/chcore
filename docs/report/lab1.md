##Chcore Lab1 实验报告

### 519021910913 黄喆敏

> 思考题 1：阅读 `_start` 函数的开头，尝试说明 ChCore 是如何让其中一个核首先进入初始化流程，并让其他核暂停执行的。

答：代码如下所示：

```assembly
	mrs	x8, mpidr_el1
	and	x8, x8,	#0xFF
	cbz	x8, primary

	/* hang all secondary processors before we introduce smp */
	b 	.
```

<img src="/Users/xtommy/Desktop/chcore-lab/docs/report/assets/lab1-1-1.png" alt="lab1-1-1" style="zoom:50%;" />

`mpidr_el1`中的Aff0位存储了core id。因此前两行将`mpidr_el1`的值存储到`x8`中，并保留低8位。若core id=0，则进入primary函数，否则执行下一行`b .`，即忙等。

即通过以上操作，将core id=0的核进入初始化流程，其他核暂停执行。



> 练习题 2：在 `arm64_elX_to_el1` 函数的 `LAB 1 TODO 1` 处填写一行汇编代码，获取 CPU 当前异常级别。
>
> 提示：通过 `CurrentEL` 系统寄存器可获得当前异常级别。通过 GDB 在指令级别单步调试可验证实现是否正确。

答：我们采用``mrs x9, CurrentEL``指令，并输出x9，获取异常级别。

​		在arm manual的C5-657页，我们可以知道如何查看CurrentEL。

<img src="/Users/xtommy/Desktop/chcore-lab/docs/report/assets/lab1-2-1.png" alt="lab1-2-1" style="zoom:50%;" />

​		接着通过gdb调试，可知x9的值为0xc。**因此当前异常级别为EL3。**

<img src="/Users/xtommy/Desktop/chcore-lab/docs/report/assets/lab1-2-2.png" alt="lab1-2-2" style="zoom:35%;" />



> 练习题 3：在 `arm64_elX_to_el1` 函数的 `LAB 1 TODO 2` 处填写大约 4 行汇编代码，设置从 EL3 跳转到 EL1 所需的 `elr_el3` 和 `spsr_el3` 寄存器值。具体地，我们需要在跳转到 EL1 时暂时屏蔽所有中断、并使用内核栈（`sp_el1` 寄存器指定的栈指针）。

答：跳转到EL1时，`elr_el3`中保存Ltarget所在地址，且`spsr_el3`中需要保存EL3的处理器状态，即`SPSR_ELX_DAIF | SPSR_ELX_EL1H`。

​		因此代码为：

```assembly
adr x9, .Ltarget
msr elr_el3, x9
mov x9, SPSR_ELX_DAIF | SPSR_ELX_EL1H
msr spsr_el3, x9
```

​		我们采用gdb单步调试，`arm64_elX_to_el1`可以正常返回到`_start`，因此代码正确。

<img src="/Users/xtommy/Desktop/chcore-lab/docs/report/assets/lab1-3-1.png" alt="lab1-3-1" style="zoom:35%;" />



> 思考题 4：结合此前 ICS 课的知识，并参考 `kernel.img` 的反汇编（通过 `aarch64-linux-gnu-objdump -S` 可获得），说明为什么要在进入 C 函数之前设置启动栈。如果不设置，会发生什么？

答：因为C函数需要利用栈，进行传递参数，保存/恢复寄存器，保存返回地址等工作。因此首先要设置足够大小的启动栈，使程序正常执行；若不设置，上述工作无法执行，可能会出现栈溢出，程序崩溃等问题。



> 思考题 5：在实验 1 中，其实不调用 `clear_bss` 也不影响内核的执行，请思考不清理 `.bss` 段在之后的何种情况下会导致内核无法工作。

答：`.bss`段包含了未初始化的全局变量和静态变量，其初始值是未知的，因此必须要清零。若不清理`.bss`段，在运行后续代码时变量的值可能不为0，可能会导致一些潜在的错误，内核无法工作。



> 练习题 6：在 `kernel/arch/aarch64/boot/raspi3/peripherals/uart.c` 中 `LAB 1 TODO 3` 处实现通过 UART 输出字符串的逻辑。

答：将每个字符依次输出即可。

```c++
void uart_send_string(char *str)
{
        for(int i = 0; str[i] != '\0'; i++) {
                early_uart_send((unsigned int) str[i]);
        }
}
```

> 练习题 7：在 `kernel/arch/aarch64/boot/raspi3/init/tools.S` 中 `LAB 1 TODO 4` 处填写一行汇编代码，以启用 MMU。

答：SCTLR_EL1寄存器的M字段用来启用/禁用MMU。因此我们采用`orr x8, x8, #SCTLR_EL1_M`指令，将M字段设为1，启用MMU。

​		我们在QEMU中验证。可以发现执行流在`0x200`处无限循环，符合要求。

<img src="/Users/xtommy/Desktop/chcore-lab/docs/report/assets/lab1-7-1.png" alt="lab1-7-1" style="zoom:35%;" />