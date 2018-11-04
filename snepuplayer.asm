.include "/usr/share/avra/m8def.inc"

.def prod_low    = r0
.def prod_high   = r1

; SN79489 registers
.def tone_0      = r2
.def tone_1      = r3
.def tone_2      = r4
.def noise       = r5
.def counter_0   = r6
.def counter_1   = r7
.def counter_2   = r8
.def counter_3   = r9
.def lfsr_l      = r10
.def lfsr_h      = r11

.def volume_0    = r16
.def volume_1    = r17
.def volume_2    = r18
.def volume_3    = r19
.def output_0    = r20
.def output_1    = r21
.def output_2    = r22
.def output_3    = r23
.def output      = r24
.def output_lfsr = r25
.def do_update   = r26
.def tick_count  = r27
.def tick_goal   = r28
.def delay_ms    = r29

.def tick_temp   = r30
.def main_temp   = r31

.org 0x0000 ; Reset
    rjmp Init

.org 0x0006 ; Timer1 Compare Match A
    rjmp Tick

.org 0x0012
    reti    ; Catch-all for unexpected interrupts

    ; Rather than ticking at the rate of a real Master System,
    ; simulate the PSG at one quarter its normal speed. This
    ; allows us to use 8-bit timers in place of 10-bit timers.
Tick:
    inc     tick_count

Tone0_decrement:
    ; ~137 cycles remaining
    tst     counter_0
    breq    Tone0_toggle
    dec     counter_0

Tone0_toggle:
    ; ~134 cycles remaining
    ; TODO: When tone=0, output=1
    tst     counter_0
    brne    Tone1_decrement
    mov     counter_0,  tone_0
    neg     output_0
    ldi     do_update,  0x01

Tone1_decrement:
    ; ~131 cycles remaining
    tst     counter_1
    breq    Tone1_toggle
    dec     counter_1

Tone1_toggle:
    ; ~128 cycles remaining
    tst     counter_1
    brne    Tone2_decrement
    mov     counter_1,  tone_1
    neg     output_1
    ldi     do_update,  0x01

Tone2_decrement:
    ; ~125 cycles remaining
    tst     counter_2
    breq    Tone2_toggle
    dec     counter_2

Tone2_toggle:
    ; ~122 cycles remaining
    tst     counter_2
    brne    Noise_decrement
    mov     counter_2,  tone_2
    neg     output_2
    ldi     do_update,  0x01

Noise_decrement:
    ; TODO: Implement the noise channel

Update:
    ; ~119 cycles remaining
    tst     do_update
    breq    Tick_done
    clr     do_update
    muls    output_0,   volume_0 ; The real PSG inverts the volume, so pre-
    mov     output,     prod_low ; process tracks so we don't have to.
    muls    output_1,   volume_1
    add     output,     prod_low
    muls    output_2,   volume_2
    add     output,     prod_low
    ; TODO: Convert signed to unsigned with an offset
    ;       -128 ->   0
    ;          0 -> 128
    ;       +127 -> 255
    ; TODO: Save components by using PWM for output
    out     PortC,      output

Tick_done:
    reti

Delay:
    ; Wait for 56 interrupts per ms
    mov     tick_goal,  tick_count
    ldi     main_temp,  56
    add     tick_goal,  main_temp

Delay_keep_waiting:
    cp      tick_goal,  tick_count
    brne    Delay_keep_waiting
    dec     delay_ms
    brne    Delay
    ret

Init:
    ; Set up the stack pointer
    ldi     main_temp,  low(RAMEND)
    out     spl,        main_temp
    ldi     main_temp,  high(RAMEND)
    out     sph,        main_temp

    ; Set Port C as six-bit output
    ldi     main_temp,  0x3f
    out     DDRC,       main_temp

    ; Timer1: CTC mode with no prescaling
    ldi     main_temp,  0x00
    out     TCCR1A,     main_temp
    ldi     main_temp,  0x09
    out     TCCR1B,     main_temp

    ; Timer1: Count to 143
    ;   We will tick at one quarter the rate of the SMS's PSG,
    ldi     main_temp,  0x00
    out     OCR1AH,     main_temp
    ldi     main_temp,  0x8f
    out     OCR1AL,     main_temp

    ; Unmask the Compare Match A interrupt
    ldi     main_temp,  0x10
    out     TIMSK,      main_temp

    ; Initial values for SN79489
    ldi     output_0,   0x01
    ldi     output_1,   0xff
    ldi     output_2,   0x01
    ldi     output_3,   0xff

    ; Enable interrupts
    sei

Main:
    ; For now, toggle between A and C

    ; A ~440 Hz
    ldi     main_temp,  0x03
    mov     volume_0,   main_temp
    ldi     main_temp,  0x40
    mov     tone_0,     main_temp

    ldi     delay_ms,   250
    rcall   Delay

    ; C ~523 Hz
    ldi     main_temp,  0x01
    mov     volume_0,   main_temp
    ldi     main_temp,  0x35
    mov     tone_0,     main_temp

    ldi     delay_ms,   250
    rcall   Delay

    rjmp    Main
