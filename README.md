mkeykernel
=======

Kernel minimal com suporte a teclado e jogo da cobrinha.

- **Start do jogo**: digitar `start`
- **Controles**: `w` `a` `s` `d`
- **Objetivo**: comer `@` para crescer. Fim de jogo ao bater nas bordas ou no próprio corpo.

#### Dependências ####
- NASM
- GCC i686 (cross ou `-m32`) e binutils (`ld`) para 32-bit
- QEMU (`qemu-system-i386`)

No Windows, recomenda-se MSYS2/MinGW ou WSL. Em MSYS2, instale: `pacman -S nasm mingw-w64-i686-gcc mingw-w64-i686-binutils qemu`.

#### Build ####
```
nasm -f elf32 kernel.asm -o kasm.o
gcc -m32 -ffreestanding -fno-stack-protector -nostdlib -c kernel.c -o kc.o
ld -m elf_i386 -T link.ld -o kernel kasm.o kc.o
```

Se surgir referência a `__stack_chk_fail`, utilize `-fno-stack-protector` como acima.

#### Rodar no QEMU ####
```
qemu-system-i386 -kernel kernel
```

Ao iniciar, digite `start` para começar.

#### Estrutura ####
- `kernel.asm`: boot + IDT/ISR ponteiro
- `kernel.c`: kernel, ISR do teclado e jogo Snake
- `keyboard_map.h`: mapa de teclas
- `link.ld`: linker script

#### Observações ####
- Modo texto VGA 80x25
- Sem comentários no código, conforme requisito
