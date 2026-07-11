.intel_syntax noprefix

.global isr_common
.extern MyPanicC

/* =====================================================================
   1. MACROS DE GÉNÉRATION DES ISR
   ===================================================================== */

/* Pour les exceptions SANS code d'erreur (ex: #DE, #UD) */
.macro ISR_NO_ERR vec
.global isr_\vec
isr_\vec:
    push 0                  /* Faux code d'erreur */
    push \vec               /* Numéro du vecteur */
    jmp isr_common
.endm

/* Pour les exceptions AVEC code d'erreur (ex: #GP, #PF) */
.macro ISR_ERR vec
.global isr_\vec
isr_\vec:
    push \vec               /* Le CPU a déjà poussé l'error code, on pousse juste le vecteur */
    jmp isr_common
.endm

/* =====================================================================
   2. LES 32 VECTEURS DU PROCESSEUR
   ===================================================================== */
ISR_NO_ERR 0   /* #DE : Division par zéro */
ISR_NO_ERR 1   /* #DB : Debug */
ISR_NO_ERR 2   /* NMI */
ISR_NO_ERR 3   /* #BP : Breakpoint */
ISR_NO_ERR 4   /* #OF : Overflow */
ISR_NO_ERR 5   /* #BR : Bound Range Exceeded */
ISR_NO_ERR 6   /* #UD : Opcode Invalide (ud2) */
ISR_NO_ERR 7   /* #NM : Device Not Available */
ISR_ERR    8   /* #DF : Double Fault */
ISR_NO_ERR 9   /* Coprocessor Segment Overrun */
ISR_ERR    10  /* #TS : Invalid TSS */
ISR_ERR    11  /* #NP : Segment Not Present */
ISR_ERR    12  /* #SS : Stack Segment Fault */
ISR_ERR    13  /* #GP : General Protection Fault */
ISR_ERR    14  /* #PF : Page Fault */
ISR_NO_ERR 15  /* Réservé / Exception Spacieuse Intel */
ISR_NO_ERR 16  /* #MF : Erreur de virgule flottante x87 (Math Fault) */
ISR_ERR    17  /* #AC : Erreur d'alignement (Alignment Check - Avec code d'erreur !) */
ISR_NO_ERR 18  /* #MC : Machine Check (Erreur matérielle interne) */
ISR_NO_ERR 19  /* #XM : Exception SIMD à virgule flottante (SSE) */
ISR_NO_ERR 20  /* #VE : Exception de virtualisation */
ISR_ERR    21  /* #CP : Exception de protection du flux de contrôle (Avec code d'erreur !) */
ISR_NO_ERR 22  /* Réservé */
ISR_NO_ERR 23  /* Réservé */
ISR_NO_ERR 24  /* Réservé */
ISR_NO_ERR 25  /* Réservé */
ISR_NO_ERR 26  /* Réservé */
ISR_NO_ERR 27  /* Réservé */
ISR_NO_ERR 28  /* Réservé */
ISR_ERR    29  /* #VC : Exception de communication VMM (Avec code d'erreur !) */
ISR_ERR    30  /* #SX : Exception de sécurité (Security Exception - Avec code d'erreur !) */
ISR_NO_ERR 31  /* Réservé */

/* =====================================================================
   3. LE GESTIONNAIRE CENTRALISÉ
   ===================================================================== */
isr_common:
    /* 1. Push general purpose registers */
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8

    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    /* 2. Push control registers */
    mov rax, cr4
    push rax
    mov rax, cr3
    push rax
    mov rax, cr2
    push rax
    mov rax, cr0
    push rax

    /* 3. Secure Stack Alignment & Parameter Passing */
    mov rbx, rsp          
    
    mov rbp, rsp          
    and rsp, -16         /* Note: ~0xF in gas intel syntax can be written as -16 */
    sub rsp, 32           

    mov rcx, rbx          
    call MyPanicC         

    /* In case MyPanicC returns, restore state cleanly */
    mov rsp, rbp
    
    /* Pop control registers */
    pop rax
    mov cr0, rax
    pop rax
    mov cr2, rax
    pop rax
    mov cr3, rax
    pop rax
    mov cr4, rax

    /* Pop general purpose registers */
    pop rax
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    add rsp, 16           
    iretq