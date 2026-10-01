; 6502/65C02 ISA vectors — comment DSL (@test / @init / @expect / @mem)
; Assemble: asmx -C 65C02 -e -w -l test.lst -o test.bin -b 0x200 test.asm
; Halt = STP opcode $DB (asmx STP mnemonic is 65C816-only → DB $DB)
        CPU     65C02
        ORG     $0200

; @test nop
; @expect pc=$0202
nop_test:
        NOP
        DB      $DB

; @test lda_imm
; @expect a=$42 flag.z=0 flag.n=0
lda_imm:
        LDA     #$42
        DB      $DB

; @test lda_imm_zero
; @expect a=$00 flag.z=1 flag.n=0
lda_imm_zero:
        LDA     #$00
        DB      $DB

; @test lda_imm_neg
; @expect a=$80 flag.z=0 flag.n=1
lda_imm_neg:
        LDA     #$80
        DB      $DB

; @test sta_zp
; @init a=$55
; @expect_mem $0010=$55
sta_zp:
        STA     $10
        DB      $DB

; @test lda_zp
; @mem $0020=$99
; @expect a=$99
lda_zp:
        LDA     $20
        DB      $DB

; @test lda_abs
; @mem $1234=$AB
; @expect a=$AB
lda_abs:
        LDA     $1234
        DB      $DB

; @test sta_abs
; @init a=$CD
; @expect_mem $2000=$CD
sta_abs:
        STA     $2000
        DB      $DB

; @test lda_abs_x
; @init x=$05
; @mem $1005=$77
; @expect a=$77
lda_abs_x:
        LDA     $1000,X
        DB      $DB

; @test lda_ind_x
; @init x=$04
; @mem $0014=$00 $0015=$30 $3000=$EE
; @expect a=$EE
lda_ind_x:
        LDA     ($10,X)
        DB      $DB

; @test lda_ind_y
; @init y=$03
; @mem $0010=$00 $0011=$40 $4003=$11
; @expect a=$11
lda_ind_y:
        LDA     ($10),Y
        DB      $DB

; @test tax_tay
; @init a=$5A
; @expect x=$5A y=$5A
tax_tay:
        TAX
        TAY
        DB      $DB

; @test txa_tya
; @init x=$11 y=$22
; @expect a=$22
txa_tya:
        TXA
        TYA
        DB      $DB

; @test adc_imm
; @init a=$10
; @expect a=$15 flag.c=0 flag.z=0
adc_imm:
        CLC
        ADC     #$05
        DB      $DB

; @test adc_carry
; @init a=$FF
; @expect a=$00 flag.c=1 flag.z=1
adc_carry:
        CLC
        ADC     #$01
        DB      $DB

; @test sbc_imm
; @init a=$10
; @expect a=$0C flag.c=1
sbc_imm:
        SEC
        SBC     #$04
        DB      $DB

; @test and_imm
; @init a=$FF
; @expect a=$0F
and_imm:
        AND     #$0F
        DB      $DB

; @test ora_imm
; @init a=$F0
; @expect a=$F5
ora_imm:
        ORA     #$05
        DB      $DB

; @test eor_imm
; @init a=$FF
; @expect a=$0F
eor_imm:
        EOR     #$F0
        DB      $DB

; @test cmp_eq
; @init a=$42
; @expect flag.z=1 flag.c=1 flag.n=0
cmp_eq:
        CMP     #$42
        DB      $DB

; @test cmp_lt
; @init a=$10
; @expect flag.z=0 flag.c=0
cmp_lt:
        CMP     #$20
        DB      $DB

; @test cpx_imm
; @init x=$05
; @expect flag.z=1 flag.c=1
cpx_imm:
        CPX     #$05
        DB      $DB

; @test cpy_imm
; @init y=$08
; @expect flag.z=0 flag.c=1
cpy_imm:
        CPY     #$03
        DB      $DB

; @test inx_iny
; @init x=$01 y=$02
; @expect x=$02 y=$03
inx_iny:
        INX
        INY
        DB      $DB

; @test dex_dey
; @init x=$01 y=$00
; @expect x=$00 y=$FF flag.n=1
dex_dey:
        DEX
        DEY
        DB      $DB

; @test inc_zp
; @mem $0030=$7F
; @expect_mem $0030=$80 flag.n=1
inc_zp:
        INC     $30
        DB      $DB

; @test dec_zp
; @mem $0031=$01
; @expect_mem $0031=$00 flag.z=1
dec_zp:
        DEC     $31
        DB      $DB

; @test asl_a
; @init a=$40
; @expect a=$80 flag.c=0 flag.n=1
asl_a:
        ASL
        DB      $DB

; @test lsr_a
; @init a=$03
; @expect a=$01 flag.c=1
lsr_a:
        LSR
        DB      $DB

; @test rol_a_c
; @init a=$80
; @expect a=$00 flag.c=1 flag.z=1
rol_a_c:
        CLC
        ROL
        DB      $DB

; @test ror_a
; @init a=$01
; @expect a=$00 flag.c=1
ror_a:
        CLC
        ROR
        DB      $DB

; @test bit_zp
; @init a=$0F
; @mem $0040=$C0
; @expect flag.z=1 flag.n=1 flag.v=1
bit_zp:
        BIT     $40
        DB      $DB

; @test pha_pla
; @init a=$A5 sp=$FD
; @expect a=$A5 sp=$FD
pha_pla:
        PHA
        LDA     #$00
        PLA
        DB      $DB

; @test php_plp
; @init a=$00
; @expect flag.z=1
php_plp:
        LDA     #$00
        PHP
        LDA     #$FF
        PLP
        DB      $DB

; @test jmp_abs
; @expect a=$99
jmp_abs:
        JMP     jmp_abs_ok
        LDA     #$00
        DB      $DB
jmp_abs_ok:
        LDA     #$99
        DB      $DB

; @test jsr_rts
; @expect a=$33
jsr_rts:
        JSR     jsr_fn
        DB      $DB
jsr_fn:
        LDA     #$33
        RTS

; @test beq_taken
; @init a=$00
; @expect a=$AA
beq_taken:
        LDA     #$00
        BEQ     beq_ok
        LDA     #$00
        DB      $DB
beq_ok:
        LDA     #$AA
        DB      $DB

; @test bne_not_taken
; @init a=$01
; @expect a=$BB
bne_not_taken:
        LDA     #$01
        BNE     bne_ok
        LDA     #$00
        DB      $DB
bne_ok:
        LDA     #$BB
        DB      $DB

; @test bcc_taken
; @expect a=$CC
bcc_taken:
        CLC
        BCC     bcc_ok
        LDA     #$00
        DB      $DB
bcc_ok:
        LDA     #$CC
        DB      $DB

; @test bcs_taken
; @expect a=$DD
bcs_taken:
        SEC
        BCS     bcs_ok
        LDA     #$00
        DB      $DB
bcs_ok:
        LDA     #$DD
        DB      $DB

; @test clc_sec
; @expect flag.c=1
clc_sec:
        CLC
        SEC
        DB      $DB

; @test cld_sed
; @expect flag.d=1
cld_sed:
        CLD
        SED
        DB      $DB

; ---- 65C02 extras ----

; @test bra
; @expect a=$EE
bra_test:
        BRA     bra_ok
        LDA     #$00
        DB      $DB
bra_ok:
        LDA     #$EE
        DB      $DB

; @test ina_dea
; @init a=$10
; @expect a=$10
ina_dea:
        INC     A
        DEC     A
        DB      $DB

; @test stz_zp
; @mem $0050=$FF
; @expect_mem $0050=$00
stz_zp:
        STZ     $50
        DB      $DB

; @test phx_plx
; @init x=$42 sp=$FD
; @expect x=$42
phx_plx:
        PHX
        LDX     #$00
        PLX
        DB      $DB

; @test phy_ply
; @init y=$24 sp=$FD
; @expect y=$24
phy_ply:
        PHY
        LDY     #$00
        PLY
        DB      $DB

; @test bit_imm
; @init a=$0F
; @expect flag.z=1
bit_imm:
        BIT     #$F0
        DB      $DB

; @test lda_zp_ind
; @mem $0060=$00 $0061=$50 $5000=$66
; @expect a=$66
lda_zp_ind:
        LDA     ($60)
        DB      $DB

; @test stp_halts
; @expect halted=1 a=$01
stp_halts:
        LDA     #$01
        DB      $DB
