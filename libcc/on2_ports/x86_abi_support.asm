%include "on2_codecs_config.asm"

; 32/64 bit compatibility macros
;
; In general, we make the source use 64 bit syntax, then twiddle with it using
; the preprocessor to get the 32 bit syntax on 32 bit platforms.
;
%ifidn __OUTPUT_FORMAT__,elf32
%define ABI_IS_32BIT
%elifidn __OUTPUT_FORMAT__,macho32
%define ABI_IS_32BIT
%elifidn __OUTPUT_FORMAT__,win32
%define ABI_IS_32BIT
%endif

%ifdef ABI_IS_32BIT
%define rax eax
%define rbx ebx
%define rcx ecx
%define rdx edx
%define rsi esi
%define rdi edi
%define rsp esp
%define rbp ebp
%define movsxd mov
%endif


; sym()
; Return the proper symbol name for the target ABI.
;
; Certain ABIs, notably MS COFF and Darwin MACH-O, require that symbols
; with C linkage be prefixed with an underscore.
;
%ifidn   __OUTPUT_FORMAT__,elf32
%define sym(x) x
%elifidn __OUTPUT_FORMAT__,elf64
%define sym(x) x
%elifidn __OUTPUT_FORMAT__,x64
%define sym(x) x
%else
%define sym(x) _ %+ x
%endif

; arg()
; Return the address specification of the given argument
;
%ifdef ABI_IS_32BIT
  %define arg(x) [ebp+8+4*x]
%else
  ; 64 bit ABI passes arguments in registers. This is a workaround to get up
  ; and running quickly. Relies on SHADOW_ARGS_TO_STACK
  %ifidn __OUTPUT_FORMAT__,x64
    %define arg(x) [rbp+16+8*x]
  %else
    %define arg(x) [rbp-8-8*x]
  %endif
%endif

; REG_SZ_BYTES, REG_SZ_BITS
; Size of a register
%ifdef ABI_IS_32BIT
%define REG_SZ_BYTES 4
%define REG_SZ_BITS  32
%else
%define REG_SZ_BYTES 8
%define REG_SZ_BITS  64
%endif


; ALIGN_STACK <alignment> <register>
; This macro aligns the stack to the given alignment (in bytes). The stack
; is left such that the previous value of the stack pointer is the first
; argument on the stack (ie, the inverse of this macro is 'pop rsp.') 
; This macro uses one temporary register, which is not preserved, and thus
; must be specified as an argument.
%macro ALIGN_STACK 2
    mov         %2, rsp
    and         rsp, -%1                       
    sub         rsp, %1 - REG_SZ_BYTES
    push        %2
%endmacro


;
; The Microsoft assembler tries to impose a certain amount of type safety in
; its register usage. YASM doesn't recognize these directives, so we just 
; %define them away to maintain as much compatibility as possible with the
; original inline assembler we're porting from.
;
%idefine PTR
%idefine XMMWORD
%idefine MMWORD


; PIC macros
;
%ifdef ABI_IS_32BIT
  %if CONFIG_PIC=1
  %ifidn __OUTPUT_FORMAT__,elf32
    %macro GET_GOT 1
      extern _GLOBAL_OFFSET_TABLE_
      call %%get_got
      %%get_got:
      pop %1
      add %1, _GLOBAL_OFFSET_TABLE_ + $$ - %%get_got wrt ..gotpc
      %undef GLOBAL
      %define GLOBAL + %1 wrt ..gotoff
    %endmacro
  %elifidn __OUTPUT_FORMAT__,macho32
    %macro GET_GOT 1
      push %1
      call %%get_got
      %%get_got:
      pop %1
      add %1, fake_got - %%get_got
      %undef GLOBAL
      %define GLOBAL + %1 - fake_got
      %undef RESTORE_GOT
      %define RESTORE_GOT pop %1
    %endmacro
  %endif
  %endif
%else
  %macro GET_GOT 1
  %endmacro  
  %define GLOBAL wrt rip
%endif
%ifnmacro GET_GOT
    %macro GET_GOT 1
    %endmacro
    %define GLOBAL
%endif
%ifndef RESTORE_GOT
%define RESTORE_GOT
%endif

%ifdef ABI_IS_32BIT
  %macro SHADOW_ARGS_TO_STACK 1
  %endm
  %define UNSHADOW_ARGS
%else
%ifidn __OUTPUT_FORMAT__,x64
  %macro SHADOW_ARGS_TO_STACK 1 ; argc
    %if %1 > 0
        mov arg(0),rcx
    %endif
    %if %1 > 1
        mov arg(1),rdx
    %endif
    %if %1 > 2
        mov arg(2),r8
    %endif
    %if %1 > 3
        mov arg(3),r9
    %endif
  %endm
%else
  %macro SHADOW_ARGS_TO_STACK 1 ; argc
    %if %1 > 0
        push rdi
    %endif
    %if %1 > 1
        push rsi
    %endif
    %if %1 > 2
        push rdx
    %endif
    %if %1 > 3
        push rcx
    %endif
    %if %1 > 4
        push r8
    %endif
    %if %1 > 5
        push r9
    %endif
    %if %1 > 6
        mov rax,[rbp+16]
        push rax
    %endif
    %if %1 > 7
        mov rax,[rbp+24]
        push rax
    %endif
    %if %1 > 8
        mov rax,[rbp+32]
        push rax
    %endif
  %endm
%endif
  %define UNSHADOW_ARGS mov rsp, rbp
%endif


; Name of the rodata section
;
; .rodata seems to be an elf-ism, as it doesn't work on OSX.
;
%ifidn __OUTPUT_FORMAT__,macho64
%define SECTION_RODATA section .text
%elifidn __OUTPUT_FORMAT__,macho32
%macro SECTION_RODATA 0
section .text
fake_got:
%endmacro  
%else
%define SECTION_RODATA section .rodata
%endif
