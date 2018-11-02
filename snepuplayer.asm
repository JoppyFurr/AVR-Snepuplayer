.include "/usr/share/avra/m8def.inc"

.def counter_0 = r16
.def counter_1 = r17
.def counter_2 = r18
.def counter_3 = r19

.org 0x0000 ; Reset
    rjmp Init

.org 0x0006 ; Timer1 Compare Match A
    rjmp Tick

.org 0x0012
    reti    ; Catch-all for unexpected interrupts

Tick:
    ; as at test, just output a counter
    inc r20
    out PortC,  r20
    reti

Init:
    ; Set up the stack pointer
    ldi r31,    low(RAMEND)
    out spl,    r31
    ldi r31,    high(RAMEND)
    out sph,    r31

    ; Set Port C as six-bit output
    ldi r31,    0x3f
    out DDRC,   r31

    ; As a test, set a bit
    ldi r20,    0b00000001
    out PortC,  r20

    ; Timer1: CTC mode with no prescaling
    ldi r31,    0x00
    out TCCR1A, r31
    ldi r31,    0x09
    out TCCR1B, r31

    ; Timer1: Count to 36
    ldi r31,    0x00
    out OCR1AH, r31
    ldi r31,    0x24 ; At 8 MHz, this will be about 0.7% slower than a real SMS
    out OCR1AL, r31

    ; Unmask the Compare Match A interrupt
    ldi r31,    0x10
    out TIMSK,  r31

    ; Enable interrupts
    sei

Main:
    nop
    rjmp Main
